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
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>

#include <functional>
#include <variant>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Performance polling.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

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
    const auto rate = floored_divide(bytes_, gap);

    // Reset counters.
    bytes_ = zero;
    start_ = now;
    set_performance(rate);
}

// private
// Removes channel from performance tracking.
void protocol_block_in_31800::reset_performance() NOEXCEPT
{
    BC_ASSERT(stranded());
    set_performance(zero);
}

// private
// Bounces to network strand, performs computation, then calls handler.
// Channel will continue to process and count blocks while this call excecutes
// on the network strand. Timer will not be restarted until cycle completes.
void protocol_block_in_31800::set_performance(uint64_t rate) NOEXCEPT
{
    BC_ASSERT(stranded());
    performance(identifier(), rate, BIND(handle_performance, _1));
}

void protocol_block_in_31800::handle_performance(const code& ec) NOEXCEPT
{
    POST(do_handle_performance, ec);
}

// private
void protocol_block_in_31800::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // stalled_channel or slow_channel
    if (ec)
    {
        stop(ec);
        return;
    };

    // Stop performance tracking without channel stop. New work restarts.
    if (map_->empty())
    {
        reset_performance();
        return;
    }

    performance_timer_->start(BIND(handle_performance_timer, _1));
}

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    if (report_performance_)
    {
        start_ = steady_clock::now();
        performance_timer_->start(BIND(handle_performance_timer, _1));
    }

    // This subscription is asynchronous without completion handler. So there
    // is no completion time guarantee, best efforts completion only (ok).
    async_subscribe_events(BIND(handle_event, _1, _2, _3));

    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);
    get_hashes(BIND(handle_get_hashes, _1, _2));
    protocol::start();
}

void protocol_block_in_31800::handle_event(const code&,
    chaser::chase event_, chaser::link value) NOEXCEPT
{
    if (event_ == chaser::chase::unassociated)
    {
        BC_ASSERT(std::holds_alternative<chaser::header_t>(value));
        POST(handle_unassociated, std::get<chaser::header_t>(value));
    }
}

// TODO: handle chaser::chase::unassociated (new downloads).
void protocol_block_in_31800::handle_unassociated(chaser::header_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    performance_timer_->stop();
    reset_performance();
    put_hashes(map_, BIND(handle_put_hashes, _1));
    protocol::stopping(ec);
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

void protocol_block_in_31800::handle_get_hashes(const code& ec,
    const map_ptr& map) NOEXCEPT
{
    BC_ASSERT_MSG(map->size() < max_inventory, "inventory overflow");

    if (ec)
    {
        LOGF("Error getting block hashes for [" << authority() << "] "
            << ec.message());
        stop(ec);
        return;
    }

    if (map->empty())
    {
        LOGP("Exhausted block hashes at [" << authority() << "] "
            << ec.message());
        return;
    }

    POST(send_get_data, map);
}

void protocol_block_in_31800::send_get_data(const map_ptr& map) NOEXCEPT
{
    BC_ASSERT(stranded());

    map_ = map;
    SEND(create_get_data(map_), handle_send, _1);
}

void protocol_block_in_31800::handle_put_hashes(const code& ec) NOEXCEPT
{
    if (ec)
    {
        LOGF("Error putting block hashes for [" << authority() << "] "
            << ec.message());
    }
}

bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    auto& query = archive();
    const auto& block = *message->block_ptr;
    const auto hash = block.hash();
    const auto it = map_->find(hash);

    if (it == map_->end())
    {
        // Allow unrequested block, not counted toward performance.
        LOGR("Unrequested block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        return true;
    }

    code error{};
    const auto& ctx = it->context;
    const auto height = possible_narrow_cast<chaser::height_t>(ctx.height);
    if (((error = block.check())) || ((error = block.check(ctx))))
    {
        // Set stored header state to 'block_unconfirmable'.
        query.set_block_unconfirmable(query.to_header(hash));

        // Notify that a candidate is 'unchecked' (candidates reorganize).
        notify(error::success, chaser::chase::unchecked, { height });

        // TODO: include context in log message.
        LOGR("Invalid block [" << encode_hash(hash) << "] at ("
            << ctx.height << ") from [" << authority() << "] "
            << error.message());

        stop(error);
        return false;
    }

    // TODO: optimize using header_fk?
    // Commit the block (txs) to the store, failure may stall the node.
    if (query.set_link(block).is_terminal())
    {
        LOGF("Failure storing block [" << encode_hash(hash) << "] at ("
            << ctx.height << ") from [" << authority() << "] "
            << error.message());
        stop(node::error::store_integrity);
        return false;
    }

    LOGP("Downloaded block [" << encode_hash(hash) << "] at ("
        << ctx.height << ") from [" << authority() << "].");

    notify(error::success, chaser::chase::checked, { height });
    bytes_ += message->cached_size;
    map_->erase(it);

    // Get some more work from chaser.
    if (map_->empty())
    {
        LOGP("Getting more block hashes for [" << authority() << "].");
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }

    return true;
}

// private
// ----------------------------------------------------------------------------

get_data protocol_block_in_31800::create_get_data(
    const map_ptr& map) const NOEXCEPT
{
    BC_ASSERT(stranded());

    get_data getter{};
    getter.items.reserve(map->size());
    std::for_each(map->pos_begin(), map->pos_end(),
        [&](const auto& item) NOEXCEPT
        {
            // clang has emplace_back bug (no matching constructor).
            // bip144: get_data uses witness constant but inventory does not.
            getter.items.push_back({ block_type_, item.hash });
        });

    return getter;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
