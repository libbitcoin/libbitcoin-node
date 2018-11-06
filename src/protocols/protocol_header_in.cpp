/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_header_in.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "header_in"
#define CLASS protocol_header_in

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_header_in::protocol_header_in(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_timer(node, channel, false, NAME),
    node_(node),
    chain_(chain),
    header_latency_(node.node_settings().block_latency()),

    // TODO: move send_headers to a derived class protocol_header_in_70012.
    send_headers_(negotiated_version() >= version::level::bip130),

    sending_headers_(false),
    CONSTRUCT_TRACK(protocol_header_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_header_in::start()
{
    protocol_timer::start(header_latency_, BIND1(handle_timeout, _1));

    SUBSCRIBE2(headers, handle_receive_headers, _1, _2);

    send_top_get_headers(null_hash);
}

// Send get_headers sequence.
//-----------------------------------------------------------------------------

void protocol_header_in::send_top_get_headers(const hash_digest& stop_hash)
{
    const auto heights = block::locator_heights(node_.top_header().height());

    chain_.fetch_header_locator(heights,
        BIND3(handle_fetch_header_locator, _1, _2, stop_hash));
}

void protocol_header_in::send_next_get_headers(const hash_digest& start_hash)
{
    // TODO: this should generate a full locator, possibly on weak branch.
    const get_headers message{ { start_hash }, null_hash };

    SEND2(message, handle_send, _1, message.command);
}

void protocol_header_in::handle_fetch_header_locator(const code& ec,
    get_headers_ptr message, const hash_digest& stop_hash)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure generating block locator for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->start_hashes().empty())
        return;

    message->set_stop_hash(stop_hash);
    const auto& last_hash = message->start_hashes().front();

    if (stop_hash == null_hash)
    {
        LOG_DEBUG(LOG_NODE)
            << "Ask [" << authority() << "] for headers after ["
            << encode_hash(last_hash) << "]";
    }
    else
    {
        LOG_DEBUG(LOG_NODE)
            << "Ask [" << authority() << "] for headers from ["
            << encode_hash(last_hash) << "] through ["
            << encode_hash(stop_hash) << "]";
    }

    SEND2(*message, handle_send, _1, message->command);
}

// Receive headers sequence.
//-----------------------------------------------------------------------------

bool protocol_header_in::handle_receive_headers(const code& ec,
    headers_const_ptr message)
{
    if (stopped(ec))
        return false;

    // An empty headers message implies peer is not ahead.
    if (message->elements().empty())
    {
        handle_timeout(error::channel_timeout);
        return true;
    }

    reset_timer();
    store_header(0, message);
    return true;
}

void protocol_header_in::store_header(size_t index, headers_const_ptr message)
{
    const auto size = message->elements().size();
    BITCOIN_ASSERT(size != 0);

    if (index >= size)
    {
        const auto last_hash = message->elements().back().hash();

        // This logs for each channel for each header.
        LOG_VERBOSE(LOG_NODE)
            << "Processed (" << size << ") headers up to ["
            << encode_hash(last_hash) << "] from [" << authority() << "].";

        // The timer handles the case where the last header is the 2000th.
        if (size < max_get_headers)
        {
            send_send_headers();
            return;
        }

        send_next_get_headers(last_hash);
        return;
    }

    // TODO: avoid this copy.
    const auto copy = std::make_shared<const header>(message->elements()[index]);

    chain_.organize(copy,
        BIND4(handle_store_header, _1, copy, index, message));
}

void protocol_header_in::handle_store_header(const code& ec, header_const_ptr header, 
    size_t index, headers_const_ptr message)
{
    if (stopped(ec))
        return;

    const auto hash = header->hash();
    const auto encoded = encode_hash(hash);

    if (ec == error::orphan_block)
    {
        // Defer this test based on the assumption most messages are correct.
        if (!message->is_sequential())
        {
            LOG_DEBUG(LOG_NODE)
                << "Disordered headers message from [" << authority() << "]";
            stop(ec);
            return;
        }

        // Try to fill the gap between the current header tree and this header.
        LOG_DEBUG(LOG_NODE)
            << "Orphan header [" << encoded << "] from [" << authority() << "]";
        send_top_get_headers(hash);
        return;
    }
    else if (ec == error::insufficient_work)
    {
        // Store in header pool to allow longer chain to build.
        LOG_DEBUG(LOG_NODE)
            << "Pooled header [" << encoded << "] from [" << authority()
            << "] " << ec.message();
    }
    else if (ec == error::duplicate_block)
    {
        // Allow duplicate header to continue as desirable race with peers.
        LOG_VERBOSE(LOG_NODE)
            << "Rejected duplicate header [" << encoded << "] from ["
            << authority() << "]";
    }
    else if (ec)
    {
        // Invalid header from peer, disconnect.
        LOG_DEBUG(LOG_NODE)
            << "Rejected header [" << encoded << "] from [" << authority()
            << "] " << ec.message();
        stop(ec);
        return;
    }
    else
    {
        const auto state = header->metadata.state;
        BITCOIN_ASSERT(state);

        // Only log every 100th header, until current.
        size_t period = chain_.is_candidates_stale() ? 100 : 1;

        if (state->height() % period == 0)
        {
            LOG_INFO(LOG_NODE)
                << "Header #" << state->height() << " ["
                << encoded << "] from [" << authority() << "] ("
                << state->enabled_forks()
                << (state->is_under_checkpoint() ? "*" : "") << ", "
                << state->minimum_block_version() << ").";
        }
    }

    // TODO: optimize with loop?
    // Break off recursion.
    DISPATCH_CONCURRENT2(store_header, ++index, message);
}

// Subscription.
//-----------------------------------------------------------------------------

// This is called directly or by the callback (base timer and stop handler).
void protocol_header_in::handle_timeout(const code& ec)
{
    if (stopped(ec))
    {
        // This may get called more than once per stop.
        handle_stop(ec);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in header timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    // Can only end up here if we are ahead, tied or peer did not respond.
    // If we are stale should try another peer and not keep pounding this one.
    if (chain_.is_candidates_stale())
    {
        LOG_DEBUG(LOG_NODE)
            << "Peer [" << authority()
            << "] is more behind or exceeded configured header latency.";
        stop(error::channel_stopped);
        return;
    }

    // In case the last request ended at exactly 2000 headers.
    send_send_headers();

    // If we are not stale then we are either good or stalled until peer sends
    // an announcement. There is no sense pinging a broken peer, so we either
    // drop the peer after a certain mount of time (above 10 minutes) or rely
    // on other peers to keep us moving and periodically age out connections.
}

// TODO: move send_headers to a derived class protocol_header_in_70012.
void protocol_header_in::send_send_headers()
{
    // Atomically test previous value and set new value to preclude race.
    if (sending_headers_.exchange(true))
        return;

    LOG_DEBUG(LOG_NETWORK)
        << "Headers are current for peer [" << authority() << "].";

    if (send_headers_)
    {
        // Request header announcements only after becoming current.
        SEND2(send_headers{}, handle_send, _1, send_headers::command);
    }
}

void protocol_header_in::handle_stop(const code&)
{
    LOG_VERBOSE(LOG_NETWORK)
        << "Stopped header_in protocol for [" << authority() << "].";
}

} // namespace node
} // namespace libbitcoin
