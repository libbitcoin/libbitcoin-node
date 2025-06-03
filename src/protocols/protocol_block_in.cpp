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
#include <bitcoin/node/protocols/protocol_block_in.hpp>

#include <algorithm>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

// The block protocol is partially obsoleted by the headers protocol.
// Both block and header protocols conflate iterative requests and unsolicited
// announcements, which introduces several ambiguities. Furthermore inventory
// messages can contain a mix of types, further increasing complexity. Unlike
// header protocol, block protocol cannot leave annoucement disabled until
// caught up and in both cases nodes announce to peers that are not caught up.

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_block_in::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);
    SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
    SEND(create_get_inventory(), handle_send, _1);
    protocol::start();
}

// accept inventory
// ----------------------------------------------------------------------------

// Receive inventory and send get_data for all blocks that are not found.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Ignore non-block inventory.
    const auto block_count = message->count(inventory::type_id::block);
    if (is_zero(block_count))
        return true;

    // Work on only one block inventory at a time.
    if (!tracker_.ids.empty())
    {
        LOGP("Unrequested (" << block_count
            << ") block inventory from [" << authority() << "] with ("
            << tracker_.ids.size() << ") pending.");
        return true;
    }

    const auto getter = create_get_data(*message);
    LOGP("Received (" << block_count << ") block inventory from ["
        << authority() << "] with (" << getter.items.size()
        << ") new blocks.");

    // If getter is empty it may be because we have them all.
    if (getter.items.empty())
    {
        // The inventory response to get_blocks is limited to max_get_blocks.
        if (block_count == max_get_blocks)
        {
            const auto& last = message->items.back().hash;
            SEND(create_get_inventory(last), handle_send, _1);
        }

        // A non-maximal inventory has no new blocks, assume complete.
        // Inventory completeness assumes empty response if caught up at 500.
        LOGP("Completed block inventory from [" << authority() << "].");
        return true;
    }

    LOGP("Requested (" << getter.items.size() << ") blocks from ["
        << authority() << "].");

    // Track inventory and request blocks (to_hashes order is reversed).
    tracker_.announced = block_count;
    tracker_.last = getter.items.back().hash;
    tracker_.ids = to_hashes(getter);
    SEND(getter, handle_send, _1);
    return true;
}

// accept block
// ----------------------------------------------------------------------------

// Process block responses in order as dictated by tracker.
bool protocol_block_in::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& block_ptr = message->block_ptr;

    // Unrequested block, may not have been announced via inventory.
    if (tracker_.ids.find(block_ptr->get_hash()) == tracker_.ids.end())
    {
        LOGP("Unrequested block [" << encode_hash(block_ptr->get_hash())
            << "] from [" << authority() << "].");
        return true;
    }

    // Inventory backlog is limited to 500 per channel.
    organize(block_ptr, BIND(handle_organize, _1, _2, block_ptr));

    return true;
}

void protocol_block_in::handle_organize(const code& ec, size_t height,
    const chain::block::cptr& block_ptr) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    // Order is enforced by organize.
    tracker_.ids.erase(block_ptr->get_hash());

    // Must erase duplicates, handled above.
    if (ec == error::duplicate_block)
        return;

    // Assuming no store failure this is an orphan or consensus failure.
    if (ec)
    {
        if (is_zero(height))
        {
            // Many peers blindly broadcast blocks even at/above v31800, ugh.
            // If we are not caught up on headers this is useless information.
            LOGP("Block [" << encode_hash(block_ptr->get_hash()) << "] from ["
                << authority() << "] " << ec.message());
        }
        else
        {
            LOGR("Block [" << encode_hash(block_ptr->hash()) << ":" << height
                << "] from [" << authority() << "] " << ec.message());
        }

        stop(ec);
        return;
    }

    LOGP("Block [" << encode_hash(block_ptr->get_hash()) << ":" << height
        << "] from [" << authority() << "].");

    // Completion of tracked inventory.
    if (tracker_.ids.empty())
    {
        // Protocol presumes max_get_blocks unless complete.
        if (tracker_.announced == max_get_blocks)
        {
            SEND(create_get_inventory(tracker_.last), handle_send, _1);
        }
        else
        {
            // Completeness stalls if on 500 as empty message is ambiguous.
            // This is ok, since complete is not used for anything essential.
            LOGP("Completed blocks from [" << authority() << "] with ("
                << tracker_.announced << ") announced.");
        }
    }
}

// utilities
// ----------------------------------------------------------------------------

get_blocks protocol_block_in::create_get_inventory() const NOEXCEPT
{
    // Block-first sync is from the archived (strong) candidate chain.
    // All strong block branches are archived, so this will reflect latest.
    // This will bypass all blocks with candidate headers, resulting in block
    // orphans if headers-first is run followed by a restart and blocks-first.
    const auto& query = archive();
    const auto index = get_blocks::heights(query.get_top_candidate());
    return create_get_inventory(query.get_candidate_hashes(index));
}

get_blocks protocol_block_in::create_get_inventory(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_inventory(hashes{ last });
}

get_blocks protocol_block_in::create_get_inventory(
    hashes&& hashes) const NOEXCEPT
{
    if (hashes.empty())
        return {};

    if (hashes.size() == one)
    {
        LOGP("Request block inventory after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }
    else
    {
        LOGP("Request block inventory (" << hashes.size() << ") after ["
            << encode_hash(hashes.front()) << "] from ["
            << authority() << "].");
    }

    return { std::move(hashes) };
}

// This will prevent most duplicate block requests despite each channel
// synchronizing its own inventory branch from startup to complete.
get_data protocol_block_in::create_get_data(
    const inventory& message) const NOEXCEPT
{
    // bip144: get_data uses witness constant but inventory does not.

    get_data getter{};
    getter.items.reserve(message.count(type_id::block));
    for (const messages::inventory_item& item: message.items)
        if ((item.type == type_id::block) && !archive().is_block(item.hash))
            getter.items.emplace_back(block_type_, item.hash);

    getter.items.shrink_to_fit();
    return getter;
}

// static
protocol_block_in::hashmap protocol_block_in::to_hashes(
    const get_data& getter) NOEXCEPT
{
    hashmap out{};
    for (const messages::inventory_item& item: getter.items)
        out.insert(item.hash);

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
