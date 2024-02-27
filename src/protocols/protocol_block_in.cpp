/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

#include <chrono>
#include <functional>
#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

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
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (started())
        return;

    const auto& query = archive();
    const auto top = query.get_top_candidate();
    top_ = { query.get_header_key(query.to_candidate(top)), top };

    SUBSCRIBE_CHANNEL2(inventory, handle_receive_inventory, _1, _2);
    SEND1(create_get_inventory(), handle_send, _1);
    protocol::start();
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

// Receive inventory and send get_data for all blocks that are not found.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");
    constexpr auto block_id = inventory::type_id::block;

    if (stopped(ec))
        return false;

    LOGP("Received (" << message->count(block_id) << ") block inventory from ["
        << authority() << "].");

    const auto getter = create_get_data(*message);

    // If getter is empty it may be only because we have them all, so iterate.
    if (getter.items.empty())
    {
        // If the original request was maximal, we assume there are more.
        // The inv response to get_blocks is limited to max_get_blocks.
        if (message->items.size() == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (empty maximal).");
            SEND1(create_get_inventory(message->items.back().hash),
                handle_send, _1);
        }

        return true;
    }

    LOGP("Requesting (" << getter.items.size() << ") blocks from ["
        << authority() << "].");

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto tracker = std::make_shared<track>(track
    {
        getter.items.size(),
        getter.items.back().hash,
        to_hashes(getter)
    });
    BC_POP_WARNING()

    // TODO: these should be limited in quantity for DOS protection.
    // There is one block subscription for each received unexhausted inventory.
    SUBSCRIBE_CHANNEL3(block, handle_receive_block, _1, _2, tracker);
    SEND1(getter, handle_send, _1);
    return true;
}

// Process block responses in order as dictated by tracker.
bool protocol_block_in::handle_receive_block(const code& ec,
    const block::cptr& message, const track_ptr& tracker) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (stopped(ec))
        return false;

    if (tracker->hashes.empty())
    {
        LOGF("Exhausted block tracker.");
        return false;
    }

    // Alias.
    const auto& block_ptr = message->block_ptr;
    const auto& block = *block_ptr;
    const auto hash = block.hash();

    // Unrequested block, may not have been announced via inventory.
    if (tracker->hashes.back() != hash)
        return true;

    // Out of order or invalid.
    if (block.header().previous_block_hash() != top_.hash())
    {
        LOGP("Orphan block [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        return false;
    }

    // Add block at next height.
    const auto height = add1(top_.height());

    // Asynchronous organization serves all channels.
    // A job backlog will occur when organize is slower than download.
    // This is not a material issue when checkpoints bypass validation.
    // The backlog may take minutes to clear upon shutdown.
    organize(block_ptr, BIND3(handle_organize, _1, height, block_ptr));

    // Set the new top and continue. Organize error will stop the channel.
    top_ = { hash, height };

    // Order is reversed, so next is at back.
    tracker->hashes.pop_back();

    // Handle completion of the inventory block subset.
    if (tracker->hashes.empty())
    {
        // Protocol presumes max_get_blocks unless complete.
        if (tracker->announced == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (exhausted maximal).");
            SEND1(create_get_inventory(tracker->last), handle_send, _1);
        }
        else
        {
            // Completeness stalls if on 500 as empty message is ambiguous.
            // This is ok, since complete is not used for anything essential.
            complete();
        }
    }

    // Release subscription if exhausted.
    // handle_receive_inventory will restart inventory iteration.
    return !tracker->hashes.empty();
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals initial currency.
void protocol_block_in::complete() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    LOGN("Blocks from [" << authority() << "] complete at ("
        << top_.height() << ").");
}

void protocol_block_in::handle_organize(const code& ec, size_t height,
    const chain::block::cptr& block_ptr) NOEXCEPT
{
    if (ec == network::error::service_stopped || ec == error::duplicate_block)
        return;

    if (ec)
    {
        // Assuming no store failure this is a consensus failure.
        LOGR("Block [" << encode_hash(block_ptr->hash())
            << "] at (" << height << ") from [" << authority() << "] "
            << ec.message());
        stop(ec);
        return;
    }

    LOGP("Block [" << encode_hash(block_ptr->hash())
        << "] at (" << height << ") from [" << authority() << "] "
        << ec.message());
}

// private
// ----------------------------------------------------------------------------

get_blocks protocol_block_in::create_get_inventory() const NOEXCEPT
{
    // Block-first sync is from the archived (strong) candidate chain.
    // All strong block branches are archived, so this will reflect latest.
    // This will bypass all blocks with candidate headers, resulting in block
    // orphans if headers-first is run followed by a restart and blocks-first.
    const auto& query = archive();
    return create_get_inventory(query.get_candidate_hashes(get_blocks::heights(
        query.get_top_candidate())));
}

get_blocks protocol_block_in::create_get_inventory(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_inventory(hashes{ last });
}

get_blocks protocol_block_in::create_get_inventory(
    hashes&& hashes) const NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request blocks after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

// This will prevent most duplicate block requests despite each channel
// synchronizing its own inventory branch from startup to complete.
get_data protocol_block_in::create_get_data(
    const inventory& message) const NOEXCEPT
{
    // clang emplace_back bug (no matching constructor), using push_back.
    // bip144: get_data uses witness constant but inventory does not.

    get_data getter{};
    getter.items.reserve(message.count(type_id::block));
    for (const auto& item: message.items)
        if ((item.type == type_id::block) && !archive().is_block(item.hash))
            getter.items.push_back({ block_type_, item.hash });

    getter.items.shrink_to_fit();
    return getter;
}

// static
hashes protocol_block_in::to_hashes(const get_data& getter) NOEXCEPT
{
    hashes out{};
    out.resize(getter.items.size());

    // Order reversed for individual erase performance (using pop_back).
    std::transform(getter.items.rbegin(), getter.items.rend(), out.begin(),
        [](const auto& item) NOEXCEPT
        {
            return item.hash;
        });

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
