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
BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Performance polling.
// ----------------------------------------------------------------------------

void protocol_block_in::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected channel strand");

    if (stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        LOGF("Performance timer error, " << ec.message());
        stop(ec);
        return;
    }

    // Compute rate in bytes per second.
    const auto now = steady_clock::now();
    const auto gap = std::chrono::duration_cast<seconds>(now - start_).count();
    const auto rate = floored_divide(bytes_, to_unsigned(gap));

    // Reset counters and log rate.
    bytes_ = zero;
    start_ = now;
    log.fire(event_block, rate);

    // Bounces to network strand, performs work, then calls handler.
    // Channel will continue to process blocks while this call excecutes on the
    // network strand. Timer will not be restarted until this call completes.
    performance(identifier(), rate, BIND1(handle_performance, ec));
}

void protocol_block_in::handle_performance(const code& ec) NOEXCEPT
{
    POST1(do_handle_performance, ec);
}

void protocol_block_in::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected network strand");

    if (stopped())
        return;

    // stalled_channel or slow_channel
    if (ec)
    {
        LOGF("Performance action, " << ec.message());
        stop(ec);
        return;
    };

    performance_timer_->start(BIND1(handle_performance_timer, _1));
}

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");

    if (started())
        return;

    state_ = archive().get_confirmed_chain_state(config().bitcoin);
    BC_ASSERT_MSG(state_, "Store not initialized.");

    if (report_performance_)
    {
        start_ = steady_clock::now();
        performance_timer_->start(BIND1(handle_performance_timer, _1));
    }

    // There is one persistent common inventory subscription.
    SUBSCRIBE_CHANNEL2(inventory, handle_receive_inventory, _1, _2);
    SEND1(create_get_inventory(), handle_send, _1);
    protocol::start();
}

void protocol_block_in::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in");
    performance_timer_->stop();
    protocol::stopping(ec);
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

// Validation is limited to block.check() and block.check(ctx).
// Context is obtained from stored header state as blocks are out of order.
// Tx check could be short-circuited against the database but since the checks
// are fast, it is optimal to wait until block/tx accept to hit the store.
// So header.state is read and when contextual checks are complete, block is
// stored. The set of blocks is obtained from the check chaser, and reported
// against it. Stopping channels return the set. May require height and/or
// header.fk to be stored with block hash set.

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
    const auto& coin = config().bitcoin;
    const auto hash = block.hash();

    // May not have been announced (miner broadcast) or different inv.
    if (tracker->hashes.back() != hash)
        return true;

    // Out of order (orphan).
    if (block.header().previous_block_hash() != state_->hash())
    {
        // Announcements are assumed to be small in number.
        if (tracker->announced > maximum_advertisement)
        {
            // Treat as invalid inventory.
            LOGR("Orphan block inventory ["
                << encode_hash(message->block_ptr->hash()) << "] from ["
                << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }
        else
        {
            // Block announcements may come before caught-up.
            LOGP("Orphan block announcement ["
                << encode_hash(message->block_ptr->hash())
                << "] from [" << authority() << "].");
            return false;
        }
    }

    auto error = block.check();
    if (error)
    {
        LOGR("Invalid block (check) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        stop(network::error::protocol_violation);
        return false;
    }

    // Rolling forward chain_state eliminates database cost.
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    state_.reset(new chain::chain_state(*state_, block.header(), coin));
    BC_POP_WARNING()
        
    const auto context = state_->context();
    error = block.check(context);
    if (error)
    {
        LOGR("Invalid block (check(context)) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        stop(network::error::protocol_violation);
        return false;
    }

    // Populate prevouts only, internal to block.
    block.populate();

    // Populate stored missing prevouts only, not input metadata.
    auto& query = archive();
    if (!query.populate(block))
    {
        LOGR("Invalid block (populate) [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::protocol_violation);
        return false;
    }
    
    ////// TODO: also requires input metadata population.
    ////error = block.accept(context, coin.subsidy_interval_blocks,
    ////    coin.initial_subsidy());
    ////if (error)
    ////{
    ////    LOGR("Invalid block (accept) [" << encode_hash(hash)
    ////        << "] from [" << authority() << "] " << error.message());
    ////    stop(network::error::protocol_violation);
    ////    return false;
    ////}

    ////// TODO: also requires input metadata population.
    ////error = block.confirm(context);
    ////if (error)
    ////{
    ////    LOGR("Invalid block (accept) [" << encode_hash(hash)
    ////        << "] from [" << authority() << "] " << error.message());
    ////    stop(network::error::protocol_violation);
    ////    return false;
    ////}
 
    // Requires only prevout population.
    error = block.connect(context);
    if (error)
    {
        LOGR("Invalid block (connect) [" << encode_hash(hash)
            << "] from [" << authority() << "] " << error.message());
        stop(network::error::protocol_violation);
        return false;
    }

    const auto link = query.set_link(block, context);
    if (link.is_terminal())
    {
        LOGF("Store block error [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::unknown);
        return false;
    }

    if (!query.push_candidate(link))
    {
        LOGF("Push candidate error [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::unknown);
        return false;
    }

    if (!query.push_confirmed(link))
    {
        LOGF("Push confirmed error [" << encode_hash(hash)
            << "] from [" << authority() << "].");
        stop(network::error::unknown);
        return false;
    }
    
    LOGP("Block [" << encode_hash(message->block_ptr->hash()) << "] at ("
        << context.height << ") from [" << authority() << "].");

    // Accumulate byte count.
    bytes_ += message->cached_size;

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
    // handle_receive_inventory will restart inventory iteration.
    return !tracker->hashes.empty();
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals initial currency.
void protocol_block_in::current() NOEXCEPT
{
    LOGN("Blocks from [" << authority() << "] complete at ("
        << state_->height() << ").");
}

// private
// ----------------------------------------------------------------------------

get_blocks protocol_block_in::create_get_inventory() const NOEXCEPT
{
    // block sync is always CONFIRMEDs.
    return create_get_inventory(archive().get_confirmed_hashes(
        get_blocks::heights(archive().get_top_confirmed())));
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
