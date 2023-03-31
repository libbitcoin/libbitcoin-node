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

#include <utility>
#include <bitcoin/system.hpp>
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
BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_block_in::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (started())
        return;

    start_ = unix_time();
    state_ = archive().get_confirmed_chain_state(config().bitcoin);

    if (!state_)
    {
        LOGF("protocol_block_in, state not initialized.");
        return;
    }

    // There is one persistent common inventory subscription.
    SUBSCRIBE_CHANNEL2(inventory, handle_receive_inventory, _1, _2);
    SEND1(create_get_inventory(), handle_send, _1);
    protocol::start();
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

// local
inline hashes to_hashes(const get_data& getter) NOEXCEPT
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

    // Track this inventory until exhausted.
    const auto tracker = std::make_shared<track>(track
    {
        getter.items.size(),
        getter.items.back().hash,
        to_hashes(getter)
    });

    // TODO: these must be limited for DOS protection.
    // There is one block subscription for each received unexhausted inventory.
    SUBSCRIBE_CHANNEL3(block, handle_receive_block, _1, _2, tracker);
    SEND1(getter, handle_send, _1);
    return true;
}

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

    const auto& block = *message->block_ptr;
    const auto hash = block.hash();
    if (tracker->hashes.back() != hash)
    {
        // This may be for another inventory (such as annouce handler).
        // May have not been announced via inv (ie by miner).
        ////LOGP("Uncorrelated block [" << encode_hash(hash)
        ////    << "] from [" << authority() << "].");
        return true;
    }

    const auto& coin = config().bitcoin;
    const auto& checks = coin.checkpoints;
    if (block.header().previous_block_hash() != state_->hash())
    {
        // Out of order or invalid.
        if (tracker->announced > maximum_advertisement)
        {
            // Treat orphan from larger-than-announce as invalid inventory.
            LOGR("Orphan block inventory ["
                << encode_hash(message->block_ptr->hash()) << "] from ["
                << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }
        else
        {
            // Unlike headers, block announcements may come before caught-up.
            LOGP("Orphan block announcement ["
                << encode_hash(message->block_ptr->hash())
                << "] from [" << authority() << "].");
            return false;
        }
    }

    auto error = block.check();
    if (error)
    {
        // assume announced.
        LOGR("Invalid block (check) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        ////stop(network::error::protocol_violation);
        return false;
    }

    // Rolling forward chain_state eliminates database cost.
    state_.reset(new chain::chain_state(*state_, block.header(), coin));
    const auto ctx = state_->context();

    if (chain::checkpoint::is_conflict(checks, hash, ctx.height))
    {
        LOGR("Invalid block (checkpoint) [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::protocol_violation);
        return false;
    }

    auto& query = archive();
    const auto link = query.set_link(block, ctx);
    if (link.is_terminal())
    {
        // This should only be from missing parent, but guarded above.
        LOGF("Store block error [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::unknown);
        return false;
    }

    // Block must be archived for full populate (no DoS protect).
    if (!query.populate(block))
    {
        LOGR("Invalid block (populate) [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::protocol_violation);
        return false;
    }

    error = block.accept(ctx, coin.subsidy_interval_blocks,
        coin.initial_subsidy());

    if (error)
    {
        LOGR("Invalid block (accept) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        stop(network::error::protocol_violation);
        return false;
    }

    error = block.connect(ctx);
    if (error)
    {
        LOGR("Invalid block (connect) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        stop(network::error::protocol_violation);
        return false;
    }

    // This is the job of the confirmation chaser.
    if (!query.push_confirmed(link))
    {
        LOGF("Push confirmed error [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::unknown);
        return false;
    }

    // This will be incorrect with multiple peers or headers protocol.
    // query.header_records() is a weak proxy for current height (top).
    if (is_zero(ctx.height % 10'000))
    {
        const auto archive_size = query.archive_size();
        reporter::fire(event_block, ctx.height);
        reporter::fire(event_archive, archive_size);
        LOGN("BLOCK:" << ctx.height
            << " sec:" << (unix_time() - start_)
            << " txs:" << query.tx_records()
            << " arc:" << archive_size);
    }

    LOGP("Block [" << encode_hash(message->block_ptr->hash()) << "] from ["
        << authority() << "].");

    // Order is reversed, so next is at back.
    tracker->hashes.pop_back();

    // Handle completion of the inventory block subset.
    if (tracker->hashes.empty())
    {
        // Implementation presumes max_get_blocks unless complete.
        if (tracker->announced == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (exhausted maximal).");
            SEND1(create_get_inventory(tracker->last), handle_send, _1);
        }
        else
        {
            // Currency stalls if current on 500 as empty message is ambiguous.
            // This is ok, since currency is not used for anything essential.
            current();
        }
    }

    // Release subscription if exhausted.
    // This will terminate block iteration if send_headers has been sent.
    // Otherwise handle_receive_inventory will restart inventory iteration.
    return !tracker->hashes.empty();
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but thissignals initial currency.
void protocol_block_in::current() NOEXCEPT
{
    // This will be incorrect with multiple peers or headers protocol.
    // archive().header_records() is a weak proxy for current height (top).
    const auto top = archive().header_records();
    reporter::fire(event_current_blocks, top);
    LOGN("Blocks from [" << authority() << "] complete at (" << top << ").");
}

// private
// ----------------------------------------------------------------------------

get_blocks protocol_block_in::create_get_inventory() const NOEXCEPT
{
    // block sync is always CONFIRMEDs.
    const auto top = archive().get_top_confirmed();
    return create_get_inventory(archive().get_confirmed_hashes(
        get_blocks::heights(top)));
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

get_data protocol_block_in::create_get_data(
    const inventory& message) const NOEXCEPT
{
    get_data getter{};
    getter.items.reserve(message.count(type_id::block));

    // clang emplace_back bug (no matching constructor), using push_back.
    // bip144: get_data uses witness constant but inventory does not.
    for (const auto& item: message.items)
        if ((item.type == type_id::block) && !archive().is_block(item.hash))
            getter.items.push_back({ block_type_, item.hash });

    getter.items.shrink_to_fit();
    return getter;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
