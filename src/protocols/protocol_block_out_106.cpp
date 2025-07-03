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
#include <bitcoin/node/protocols/protocol_block_out_106.hpp>

#include <chrono>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_out_106

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_block_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));
    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    SUBSCRIBE_CHANNEL(get_blocks, handle_receive_get_blocks, _1, _2);
    protocol::start();
}

void protocol_block_out_106::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (block)
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped() || superseded())
        return false;

    switch (event_)
    {
        case chase::block:
        {
            // value is organized block pk.
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_announce, std::get<header_t>(value));
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Outbound (block).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::superseded() const NOEXCEPT
{
    return false;
}

bool protocol_block_out_106::do_announce(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return false;

    // Don't announce to peer that announced to us.
    const auto hash = archive().get_header_key(link);
    if (was_announced(hash))
        return true;

    if (hash == null_hash)
    {
        ////stop(fault(system::error::not_found));
        LOGF("Organized block not found.");
        return true;
    }

    // bip144: get_data uses witness type_id but inv does not.
    const inventory inv{ { { type_id::block, hash } } };
    SEND(inv, handle_send, _1);
    return true;
}

// Inbound (get_blocks).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_receive_get_blocks(const code& ec,
    const get_blocks::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    LOGP("Get headers above " << encode_hash(message->start_hash())
        << " from [" << authority() << "].");

    SEND(create_inventory(*message), handle_send, _1);
    return true;
}

// Inbound (get_data).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_receive_get_data(const code& ec,
    const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // Send and desubscribe.
    send_block(error::success, zero, message);
    return false;
}

// Outbound (block).
// ----------------------------------------------------------------------------

void protocol_block_out_106::send_block(const code& ec, size_t index,
    const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // Skip over non-block inventory.
    for (; index < message->items.size(); ++index)
        if (message->items.at(index).is_block_type())
            break;

    if (index >= message->items.size())
    {
        // Complete, resubscribe to block requests.
        SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
        return;
    }

    const auto& item = message->items.at(index);
    const auto witness = item.is_witness_type();
    if (!node_witness_ && witness)
    {
        LOGR("Unsupported witness get_data from [" << authority() << "].");
        stop(network::error::protocol_violation);
        return;
    }

    // Block could be always queried with witness and therefore safely cached.
    // If can then be serialized according to channel configuration, however
    // that is currently fixed to witness as available in the object.
    const auto& query = archive();
    const auto start = logger::now();
    const auto ptr = query.get_block(query.to_header(item.hash), witness);
    if (!ptr)
    {
        LOGR("Requested block " << encode_hash(item.hash)
            << " from [" << authority() << "] not found.");

        // This block could not have been advertised to the peer.
        stop(system::error::not_found);
        return;
    }

    span<milliseconds>(events::getblock_msecs, start);
    SEND(block{ ptr }, send_block, _1, sub1(index), message);
}

// utilities
// ----------------------------------------------------------------------------

inventory protocol_block_out_106::create_inventory(
    const get_blocks& locator) const NOEXCEPT
{
    // Empty response implies complete (success).
    if (!is_current(true))
        return {};

    return inventory::factory
    (
        archive().get_blocks(locator.start_hashes, locator.stop_hash,
            max_get_blocks), type_id::block
    );
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
