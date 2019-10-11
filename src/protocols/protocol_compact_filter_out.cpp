/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_compact_filter_out.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <boost/range/adaptor/reversed.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "compact_filter_out"
#define CLASS protocol_compact_filter_out

using namespace bc::blockchain;
using namespace bc::network;
using namespace bc::system;
using namespace bc::system::message;
using namespace boost::adaptors;
using namespace std::placeholders;

protocol_compact_filter_out::protocol_compact_filter_out(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_events(node, channel, NAME),
    node_(node),
    chain_(chain),
    last_locator_top_(null_hash),
    CONSTRUCT_TRACK(protocol_compact_filter_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_compact_filter_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    SUBSCRIBE2(get_compact_filters, handle_receive_get_compact_filters, _1, _2);

    SUBSCRIBE2(get_compact_filter_headers,
        handle_receive_get_compact_filter_headers, _1, _2);

    SUBSCRIBE2(get_compact_filter_checkpoint,
        handle_receive_get_compact_filter_checkpoint, _1, _2);
}

// Stop.
//-----------------------------------------------------------------------------
void protocol_compact_filter_out::handle_stop(const code&)
{
    LOG_VERBOSE(LOG_NETWORK)
        << "Stopped " << NAME << " protocol for [" << authority() << "].";
}

// Receive get_compact_filter_checkpoint
//-----------------------------------------------------------------------------
bool protocol_compact_filter_out::handle_receive_get_compact_filter_checkpoint(
    const system::code& ec,
    system::get_compact_filter_checkpoint_const_ptr message)
{
    if (stopped(ec))
        return false;

    if (message)
        chain_.fetch_compact_filter_checkpoint(message->filter_type(),
            message->stop_hash(),
            BIND2(handle_compact_filter_checkpoint, _1, _2));

    return true;
}

void protocol_compact_filter_out::handle_compact_filter_checkpoint(
    const system::code& ec, system::compact_filter_checkpoint_ptr message)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating compact filter checkpoint for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->filter_headers().empty())
        return;

    // Respond to get_compact_filter_checkpoint with compact_filter_checkpoint.
    SEND2(*message, handle_send, _1, message->command);
}

// Receive get_compact_filter_headers
//-----------------------------------------------------------------------------
bool protocol_compact_filter_out::handle_receive_get_compact_filter_headers(
    const system::code& ec,
    system::get_compact_filter_headers_const_ptr message)
{
    if (stopped(ec))
        return false;

    if (message)
        chain_.fetch_compact_filter_headers(message->filter_type(),
            message->start_height(), message->stop_hash(),
            BIND2(handle_compact_filter_headers, _1, _2));

    return true;
}

void protocol_compact_filter_out::handle_compact_filter_headers(
    const system::code& ec, system::compact_filter_headers_ptr message)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating compact filter headers for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->filter_hashes().empty())
        return;

    // Respond to get_compact_filter_headers with compact_filter_headers.
    SEND2(*message, handle_send, _1, message->command);
}

// Receive get_compact_filters
//-----------------------------------------------------------------------------
bool protocol_compact_filter_out::handle_receive_get_compact_filters(
    const system::code& ec, system::get_compact_filters_const_ptr message)
{
    if (stopped(ec))
        return false;

    chain_.fetch_compact_filter(message->filter_type(),
        message->stop_hash(),
        BIND5(handle_compact_filters_start, message, _1, _2, _3, _4));

    return true;
}

void protocol_compact_filter_out::handle_compact_filters_start(
    system::get_compact_filters_const_ptr request, const system::code& ec,
    const system::hash_digest&, const system::data_chunk&,
    size_t height)
{
    if (stopped(ec))
        return;

    if ((height < request->start_height()) ||
        (height - request->start_height() > max_get_compact_filters))
    {
        LOG_ERROR(LOG_NODE)
            << "The requested range exceeds maximum for ["
            << authority() << "] " << ec.message();
        stop(error::invalid_response_range);
    }

    chain_.fetch_compact_filter(request->filter_type(),
        request->start_height(),
        BIND6(handle_next_compact_filter, request, height, _1, _2, _3, _4));
}

void protocol_compact_filter_out::handle_next_compact_filter(
    system::get_compact_filters_const_ptr request, size_t stop_height,
    const system::code& ec, const system::hash_digest& block_hash,
    const system::data_chunk& filter, size_t height)
{
    if (stopped(ec))
        return;

    message::compact_filter message(request->filter_type(), block_hash, filter);
    SEND2(message, handle_send, _1, message.command);

    if (height < stop_height)
        chain_.fetch_compact_filter(request->filter_type(),
            height + 1,
            BIND6(handle_next_compact_filter,
                request, stop_height, _1, _2, _3, _4));
}

} // namespace node
} // namespace libbitcoin
