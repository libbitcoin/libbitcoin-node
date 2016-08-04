/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_block_in.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block"
#define CLASS protocol_block_in

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

static constexpr auto perpetual_timer = true;
static constexpr auto send_headers_version = 70012u;
static const auto get_blocks_interval = asio::seconds(1);

protocol_block_in::protocol_block_in(p2p& network, channel::ptr channel,
    block_chain& blockchain)
  : protocol_timer(network, channel, perpetual_timer, NAME),
    blockchain_(blockchain),
    stop_hash_(null_hash),
    headers_from_peer_(peer_version().value >= send_headers_version),
    CONSTRUCT_TRACK(protocol_block_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_in::start()
{
    // Use perpetual protocol timer to prevent stall (our heartbeat).
    protocol_timer::start(get_blocks_interval,
        BIND1(send_get_headers_or_blocks, _1));

    SUBSCRIBE2(headers, handle_receive_headers, _1, _2);
    SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    if (headers_from_peer_)
    {
        // Allow peer to send headers vs. inventory block anncements.
        SEND2(send_headers(), handle_send, _1, send_headers::command);
    }

    // Subscribe to block acceptance notifications (for gap fill redundancy).
    blockchain_.subscribe_reorganize(
        BIND4(handle_reorganized, _1, _2, _3, _4));

    // Send initial get_[blocks|headers] message by simulating first heartbeat.
    set_event(error::success);
}

// Send get_[headers|blocks] sequence.
//-----------------------------------------------------------------------------

// This is fired by the callback (i.e. base timer and stop handler).
void protocol_block_in::send_get_headers_or_blocks(const code& ec)
{
    if (stopped())
        return;

    if (ec && ec != error::channel_timeout)
    {
        log::debug(LOG_NODE)
            << "Failure in block timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    blockchain_.fetch_block_locator(
        BIND2(handle_fetch_block_locator, _1, _2));
}

void protocol_block_in::handle_fetch_block_locator(const code& ec,
    const hash_list& locator)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure generating block locator for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: manage the stop_hash_ (see v2).
    ///////////////////////////////////////////////////////////////////////////

    if (headers_from_peer_)
    {
        const get_headers request{ std::move(locator), stop_hash_ };
        SEND2(request, handle_send, _1, request.command);
    }
    else
    {
        const get_blocks request{ std::move(locator), stop_hash_ };
        SEND2(request, handle_send, _1, request.command);
    }
}

// Receive headers|inventory sequence.
//-----------------------------------------------------------------------------

// This originates from send_header->annoucements and get_headers requests.
bool protocol_block_in::handle_receive_headers(const code& ec,
    message::headers::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting headers from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // There is no benefit to this use of headers, in fact it is suboptimal.
    // In v3 headers will be used to build block tree before getting blocks.
    ///////////////////////////////////////////////////////////////////////////

    hash_list block_hashes;
    message->to_hashes(block_hashes);

    // TODO: implement orphan_pool_.fetch_missing_block_hashes(...)
    handle_fetch_missing_orphans(error::success, block_hashes);
    return true;
}

// This originates from default annoucements and get_blocks requests.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    message::inventory::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting inventory from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list block_hashes;
    message->to_hashes(block_hashes, inventory_type_id::block);

    // TODO: implement orphan_pool_.fetch_missing_block_hashes(...)
    handle_fetch_missing_orphans(error::success, block_hashes);
    return true;
}

void protocol_block_in::handle_fetch_missing_orphans(const code& ec,
    const hash_list& block_hashes)
{
    if (stopped() || ec == error::service_stopped || block_hashes.empty())
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating missing orphan hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    blockchain_.fetch_missing_block_hashes(block_hashes,
        BIND2(send_get_data, _1, _2));
}

void protocol_block_in::send_get_data(const code& ec, const hash_list& hashes)
{
    if (stopped() || ec == error::service_stopped || hashes.empty())
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating missing block hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // inventory|headers->get_data[blocks]
    get_data request(hashes, inventory_type_id::block);
    SEND2(request, handle_send, _1, request.command);
}

// Receive not_found sequence.
//-----------------------------------------------------------------------------

bool protocol_block_in::handle_receive_not_found(const code& ec,
    message::not_found::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting block not_found from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list hashes;
    message->to_hashes(hashes, inventory_type_id::block);

    // The peer cannot locate a block that it told us it had.
    // This only results from reorganization assuming peer is proper.
    for (const auto hash: hashes)
    {
        log::debug(LOG_NODE)
            << "Block not_found [" << encode_hash(hash) << "] from ["
            << authority() << "]";
    }

    return true;
}

// Receive block sequence.
//-----------------------------------------------------------------------------

bool protocol_block_in::handle_receive_block(const code& ec,
    message::block::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting block from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    blockchain_.store(message, BIND1(handle_store_block, _1));
    return true;
}

void protocol_block_in::handle_store_block(const code& ec)
{
    if (stopped() || ec == error::service_stopped)
        return;

    // Ignore the block that we already have.
    if (ec == error::duplicate)
    {
        log::debug(LOG_NODE)
            << "Redundant block from [" << authority() << "] "
            << ec.message();
        return;
    }

    // Drop the channel if the block is invalid.
    if (ec)
    {
        log::warning(LOG_NODE)
            << "Invalid block from [" << authority() << "] "
            << ec.message();
        set_event(ec);
        return;
    }

    // The block is accepted as an orphan.
    // There is a DoS vector in peer repeatedly sending the same valid block.
    // We should drop channels that send "large" blocks we haven't requested.
    // We can then announce "small" blocks in place of sending header/inv.
    log::debug(LOG_NODE)
        << "Potential block from [" << authority() << "].";
}

// Subscription.
//-----------------------------------------------------------------------------

bool protocol_block_in::handle_reorganized(const code& ec, size_t fork_point,
    const block::ptr_list& incoming, const block::ptr_list& outgoing)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: send new get_[blocks\header] request (see v2).
    ///////////////////////////////////////////////////////////////////////////
    return true;
}

} // namespace node
} // namespace libbitcoin
