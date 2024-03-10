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

void protocol_block_in_31800::start_performance() NOEXCEPT
{
    if (stopped())
        return;

    if (report_performance_)
    {
        bytes_ = zero;
        start_ = steady_clock::now();
        performance_timer_->start(BIND(handle_performance_timer, _1));
    }
}

void protocol_block_in_31800::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec == network::error::operation_canceled)
        return;

    if (stopped())
        return;

    if (ec)
    {
        LOGF("Performance timer failure, " << ec.message());
        stop(ec);
        return;
    }

    if (map_->empty())
    {
        // Channel is exhausted, performance no longer relevant.
        pause_performance();
        return;
    }

    const auto delta = duration_cast<seconds>(steady_clock::now() - start_);
    const auto unsigned_delta = sign_cast<uint64_t>(delta.count());
    const auto non_zero_period = system::greater(unsigned_delta, one);
    const auto rate = floored_divide(bytes_, non_zero_period);
    send_performance(rate);
}

void protocol_block_in_31800::pause_performance() NOEXCEPT
{
    send_performance(max_uint64);
}

void protocol_block_in_31800::stop_performance() NOEXCEPT
{
    send_performance(zero);
}

// [0...slow...fast] => [stalled_channel...slow_channel...success]
void protocol_block_in_31800::send_performance(uint64_t rate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (report_performance_)
    {
        performance_timer_->stop();
        performance(identifier(), rate, BIND(handle_send_performance, _1));
    }
}

void protocol_block_in_31800::handle_send_performance(const code& ec) NOEXCEPT
{
    POST(do_handle_performance, ec);
}

void protocol_block_in_31800::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // Caused only by performance(max) - had no outstanding work.
    // Timer stopped until chaser::download event restarts it.
    if (ec == error::exhausted_channel)
        return;

    // Caused only by performance(zero|xxx) - had outstanding work.
    if (ec == error::stalled_channel || ec == error::slow_channel)
    {
        LOGP("Performance [" << authority() << "] " << ec.message());
        stop(ec);
        return;
    }

    if (ec)
    {
        LOGF("Performance failure [" << authority() << "] " << ec.message());
        stop(ec);
        return;
    }

    // Restart performance timing cycle.
    start_performance();
}

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous.
    async_subscribe_events(BIND(handle_event, _1, _2, _3));
    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);

    // Start performance timing cycle.
    start_performance();
    get_hashes(BIND(handle_get_hashes, _1, _2));
    protocol::start();
}

void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    restore(map_);
    map_ = std::make_shared<database::associations>();
    stop_performance();

    protocol::stopping(ec);
}

// Idempotent cleanup.
void protocol_block_in_31800::restore(const map_ptr& map) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map->empty())
        put_hashes(map, BIND(handle_put_hashes, _1));
}

void protocol_block_in_31800::handle_event(const code&,
    chaser::chase event_, chaser::link value) NOEXCEPT
{
    if (stopped())
        return;

    // There are count blocks to download at/above the given header.
    if (event_ == chaser::chase::download)
    {
        BC_ASSERT(std::holds_alternative<chaser::count_t>(value));
        POST(handle_download, std::get<chaser::count_t>(value));
    }
}

void protocol_block_in_31800::handle_download(chaser::count_t count) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (map_->empty() && !is_zero(count))
    {
        // Assume performance was stopped due to exhaustion.
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

void protocol_block_in_31800::send_get_data(const map_ptr& map) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
    {
        restore(map);
        return;
    }

    if (map->empty())
        return;

    if (map_->empty())
    {
        SEND(create_get_data((map_ = map)), handle_send, _1);
        return;
    }

    // There are two populated maps, return the new and leave the old alone.
    restore(map);
}

// private
// clang has emplace_back bug (no matching constructor).
// bip144: get_data uses witness constant but inventory does not.
get_data protocol_block_in_31800::create_get_data(
    const map_ptr& map) const NOEXCEPT
{
    get_data getter{};
    getter.items.reserve(map->size());
    std::for_each(map->pos_begin(), map->pos_end(),
        [&](const auto& item) NOEXCEPT
        {
            getter.items.push_back({ block_type_, item.hash });
        });

    return getter;
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

    // Check block.
    // ------------------------------------------------------------------------

    code error{};
    const auto& link = it->link;
    const auto& ctx = it->context;
    const auto height = possible_narrow_cast<chaser::height_t>(ctx.height);
    if (((error = block.check())) || ((error = block.check(ctx))))
    {
        query.set_block_unconfirmable(link);
        notify(error::success, chaser::chase::unchecked, link);

        LOGR("Invalid block [" << encode_hash(hash) << "] at ("
            << ctx.height << ") from [" << authority() << "] "
            << error.message());

        stop(error);
        return false;
    }

    // Commit block.txs.
    // ------------------------------------------------------------------------

    // Commit block.txs to store, failure may stall the node.
    if (query.set_link(*block.transactions_ptr(), link).is_terminal())
    {
        LOGF("Failure storing block [" << encode_hash(hash) << "] at ("
            << ctx.height << ") from [" << authority() << "] "
            << error.message());
        stop(node::error::store_integrity);
        return false;
    }

    // ------------------------------------------------------------------------

    LOGP("Downloaded block [" << encode_hash(hash) << "] at ("
        << ctx.height << ") from [" << authority() << "].");

    notify(error::success, chaser::chase::checked, height);
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

void protocol_block_in_31800::handle_put_hashes(const code& ec) NOEXCEPT
{
    if (ec)
    {
        LOGF("Error putting block hashes for [" << authority() << "] "
            << ec.message());
    }
}

void protocol_block_in_31800::handle_get_hashes(const code& ec,
    const map_ptr& map) NOEXCEPT
{
    BC_ASSERT_MSG(map->size() < max_inventory, "inventory overflow");

    if (stopped())
    {
        restore(map);
        return;
    }

    if (ec)
    {
        LOGF("Error getting block hashes for [" << authority() << "] "
            << ec.message());
        stop(ec);
        return;
    }

    if (map->empty())
    {
        ////LOGP("Block hashes for [" << authority() << "] exhausted.");
        return;
    }

    POST(send_get_data, map);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
