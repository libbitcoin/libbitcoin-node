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
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_filter_out_70015::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_CHANNEL(get_client_filter_checkpoint, handle_receive_get_filter_checkpoint, _1, _2);
    SUBSCRIBE_CHANNEL(get_client_filter_headers, handle_receive_get_filter_headers, _1, _2);
    SUBSCRIBE_CHANNEL(get_client_filters, handle_receive_get_filters, _1, _2);
    protocol::start();
}

// Inbound (get_client_filter_checkpoint).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_filter_checkpoint(
    const code& ec, const get_client_filter_checkpoint::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // bip157: Nodes SHOULD NOT send getcfcheckpt unless peer has signaled
    // support for this filter type (the method of signal is unspecified).
    if (message->filter_type != client_filter::type_id::neutrino)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    const auto& query = archive();
    const auto start = logger::now();

    // bip157: node SHOULD NOT respond to getcfcheckpt with unknown stop_hash.
    // bip157: stop_hash MUST have been announced (or ancestor of) block hash.
    size_t stop_height{};
    const auto stop_link = query.to_header(message->stop_hash);
    if (!query.get_height(stop_height, stop_link))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // There is no guarantee that this set will be consistent across reorgs.
    // However for it to be inconsistent there must be a > 1000 block reorg.
    // If the branch has never been confirmed then filters will not be found.
    client_filter_checkpoint out{};
    if (!query.get_filter_heads(out.filter_headers, stop_height,
        client_filter_checkpoint_interval))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    out.stop_hash = message->stop_hash;
    out.filter_type = client_filter::type_id::neutrino;
    span<milliseconds>(events::filterchecks_msecs, start);
    SEND(out, handle_send, _1);
    return true;
}

// Inbound (get_client_filter_headers).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_filter_headers(
    const code& ec, const get_client_filter_headers::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // bip157: Nodes SHOULD NOT send getcfheaders unless peer has signaled
    // support for this filter type (the method of signal is unspecified).
    if (message->filter_type != client_filter::type_id::neutrino)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    const auto& query = archive();
    const auto start = logger::now();

    // bip157: node SHOULD NOT respond to getcfheaders with unknown stop_hash.
    // bip157: stop_hash MUST have been announced (or ancestor of) block hash.
    size_t stop_height{};
    const auto stop_link = query.to_header(message->stop_hash);
    if (!query.get_height(stop_height, stop_link))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // bip157: height of block with stop_hash MUST be >= to start_height.
    const size_t start_height = message->start_height;
    if (is_subtract_overflow(stop_height, start_height))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // bip157: difference [stop_height - start_height] MUST be less than 2000.
    const auto count = subtract(stop_height, start_height);
    if (count >= max_client_filter_headers)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // The response is assured to represent a consistent branch.
    client_filter_headers out{};
    if (!query.get_filter_hashes(out.filter_hashes, out.previous_filter_header,
        stop_link, count))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    out.stop_hash = message->stop_hash;
    out.filter_type = client_filter::type_id::neutrino;
    span<milliseconds>(events::filterhashes_msecs, start);
    SEND(out, handle_send, _1);
    return true;
}

// Inbound (get_client_filters).
// ----------------------------------------------------------------------------

bool protocol_filter_out_70015::handle_receive_get_filters(const code& ec,
    const get_client_filters::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // bip157: Nodes SHOULD NOT send getcfilters unless peer has signaled
    // support for this filter type (the method of signal is unspecified).
    if (message->filter_type != client_filter::type_id::neutrino)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    const auto& query = archive();
    const auto start = logger::now();

    // bip157: node SHOULD NOT respond to getcfilters with unknown stop_hash.
    // bip157: stop_hash MUST have been announced (or ancestor of) block hash.
    size_t stop_height{};
    const auto stop_link = query.to_header(message->stop_hash);
    if (!query.get_height(stop_height, stop_link))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // bip157: height of block with stop_hash MUST be >= to start_height.
    const size_t start_height = message->start_height;
    if (is_subtract_overflow(stop_height, start_height))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // bip157: difference [stop_height - start_height] MUST be less than 1000.
    const auto count = subtract(stop_height, start_height);
    if (count >= max_client_filters)
    {
        stop(network::error::protocol_violation);
        return false;
    }

    // The response is assured to represent a consistent branch.
    const auto ancestry = std::make_shared<database::header_links>();
    if (!query.get_ancestry(*ancestry, stop_link, count))
    {
        stop(network::error::protocol_violation);
        return false;
    }

    span<milliseconds>(events::ancestry_msecs, start);
    send_filter(error::success, ancestry);
    return false;
}

void protocol_filter_out_70015::send_filter(const code& ec,
    const ancestry_ptr& ancestry) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    if (ancestry->empty())
    {
        // Complete, resubscribe to get_client_filters.
        SUBSCRIBE_CHANNEL(get_client_filters, handle_receive_get_filters, _1, _2);
        return;
    }

    const auto& query = archive();
    const auto start = logger::now();
    const auto link = system::pop(*ancestry);

    client_filter out{};
    if (!query.get_filter_body(out.filter, link))
    {
        stop(network::error::protocol_violation);
        return;
    }

    out.block_hash = query.get_header_key(link);
    out.filter_type = client_filter::type_id::neutrino;
    span<milliseconds>(events::filter_msecs, start);
    SEND(out, send_filter, _1, ancestry);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
