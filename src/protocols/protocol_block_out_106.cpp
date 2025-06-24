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

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_out_106

using namespace system;
using namespace network::messages;
using namespace std::placeholders;

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_block_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    SUBSCRIBE_CHANNEL(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE_BROADCAST(block, handle_broadcast_block, _1, _2, _3);
    protocol::start();
}

// Outbound (block).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_broadcast_block(const code& ec,
    const block::cptr& message, uint64_t sender) NOEXCEPT
{
    BC_ASSERT(stranded());

    // False return unsubscribes this broadcast handler.
    if (stopped(ec) || disabled())
        return false;

    if (sender == identifier())
        return true;

    const inventory inv{ { { type_id::block, message->block_ptr->hash() } } };
    SEND(inv, handle_send, _1);
    return true;
}

bool protocol_block_out_106::disabled() const NOEXCEPT
{
    return false;
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

    if (index >= message->items.size())
    {
        // Complete, resubscribe to block requests.
        SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
        return;
    }

    const auto& query = archive();
    const auto& hash = message->items.at(index).hash;
    const auto block_ptr = query.get_block(query.to_header(hash));

    if (!block_ptr)
    {
        LOGR("Requested block not found " << encode_hash(hash)
            << " from [" << authority() << "].");

        // This block could not have been advertised to the peer.
        stop(system::error::not_found);
        return;
    }

    SEND(block{ block_ptr }, send_block, _1, sub1(index), message);
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
            max_get_blocks), inventory::type_id::block
    );
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
