/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_filter_out_70015.hpp>

#include <chrono>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_filter_out_70015

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::chrono;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_filter_out_70015::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_CHANNEL(get_client_filter_checkpoint,
        handle_receive_get_client_filter_checkpoint, _1, _2);
    SUBSCRIBE_CHANNEL(get_client_filter_headers,
        handle_receive_get_client_filter_headers, _1, _2);
    SUBSCRIBE_CHANNEL(get_client_filters,
        handle_receive_get_client_filters, _1, _2);
    protocol::start();
}

// Inbound (get_client_filter_checkpoint).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_client_filter_checkpoint(
    const code& ec, const get_client_filter_checkpoint::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // TODO:
    SEND(client_filter_checkpoint{}, handle_send, _1);
    return true;
}

// Inbound (get_client_filter_headers).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_client_filter_headers(
    const code& ec, const get_client_filter_headers::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // TODO:
    SEND(client_filter_headers{}, handle_send, _1);
    return true;
}

// Inbound (get_client_filters).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_client_filters(
    const code& ec, const get_client_filters::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // bip157: Nodes SHOULD NOT send getcfilters unless peer has signaled
    // support for this filter type (the method of signal is unspecified).
    const auto& query = archive();
    if (message->filter_type != client_filter::type_id::neutrino)
        return true;

    // bip157: node SHOULD NOT respond to getcfilters with unknown stop_hash.
    size_t stop_height{};
    if (!query.get_height(stop_height, query.to_header(message->stop_hash)))
        return true;

    // bip157: height of block with stop_hash MUST be >= to start_height.
    // bip157: difference [stop_height - start_height] MUST be less than 1000.
    const size_t start_height = message->start_height;
    if (is_subtract_overflow(stop_height, start_height) ||
        subtract(stop_height, start_height) >= max_client_filter_request)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // Send and desubscribe.
    // bip157: node MUST respond by sending one cfilter message for each block
    // in the requested range, sequentially in order by block height.
    send_client_filter(error::success, start_height, stop_height, message);
    return false;
}

void protocol_filter_out_70015::send_client_filter(const code& ec, size_t height,
    size_t stop_height, const get_client_filters::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // Forward ordering by height makes it costly to ensure consistency when
    // spanning a reorganization, so allow that to pass to client for handling.
    const auto& query = archive();
    const auto start = logger::now();

    client_filter out{};
    if (!query.get_filter_body(out.filter, query.to_confirmed(height)))
    {
        LOGF("Filter at (" << height << ") not found.");
        stop(system::error::not_found);
        return;
    }

    span<milliseconds>(events::getfilter_msecs, start);

    if (height == stop_height)
    {
        // Complete, resubscribe to get_client_filters.
        SUBSCRIBE_CHANNEL(get_client_filters,
            handle_receive_get_client_filters, _1, _2);
        return;
    }

    SEND(out, send_client_filter, _1, add1(height), stop_height, message);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
