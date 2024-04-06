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

#include <algorithm>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// start/stop
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous.
    async_subscribe_events(BIND(handle_event, _1, _2, _3));
    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);

    // Start performance timing and download cycles if candidates are current.
    // This prevents a startup delay in which the node waits on a header.
    if (is_current())
    {
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }

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

// handle events (download, split)
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::is_idle() const NOEXCEPT
{
    return map_->empty();
}

void protocol_block_in_31800::handle_event(const code&,
    chase event_, event_link value) NOEXCEPT
{
    using namespace system;
    if (stopped())
        return;

    switch (event_)
    {
        case chase::pause:
        {
            // Pause local timers due to channel pause (e.g. snapshot pending).
            POST(do_pause, channel_t{});
            break;
        }
        case chase::resume:
        {
            // Resume local timers due to channel resume (e.g. snapshot done).
            POST(do_resume, channel_t{});
            break;
        }
        case chase::split:
        {
            // It was determined to be the slowest channel with work.
            // If value identifies this channel, split work and stop.
            if (possible_narrow_cast<channel_t>(value) == identifier())
            {
                POST(do_split, channel_t{});
            }

            break;
        }
        case chase::stall:
        {
            // If this channel has divisible work, split it and stop.
            // There are no channels reporting work, either stalled or done.
            // This is initiated by any channel notifying chase::starved.
            if (map_->size() >= minimum_for_stall_divide)
            {
                POST(do_split, channel_t{});
            }

            break;
        }
        case chase::purge:
        {
            // If this channel has work clear it and stop.
            // This is initiated by validate/confirm chase::disorganized.
            if (map_->size() >= minimum_for_stall_divide)
            {
                POST(do_purge, channel_t{});
            }

            break;
        }
        case chase::download:
        {
            // There are count blocks to download at/above given header.
            // But don't download blocks until candidate chain is current.
            if (is_current())
            {
                POST(do_get_downloads, count_t{});
            }

            break;
        }
        case chase::start:
        ////case chase::pause:
        ////case chase::resume:
        case chase::starved:
        ////case chase::split:
        ////case chase::stall:
        ////case chase::purge:
        case chase::block:
        case chase::header:
        ////case chase::download:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmable:
        case chase::unpreconfirmable:
        case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        case chase::disorganized:
        case chase::malleated:
        case chase::transaction:
        case chase::template_:
        case chase::stop:
        {
            break;
        }
    }
}

void protocol_block_in_31800::do_get_downloads(count_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (is_idle())
    {
        // Assume performance was stopped due to exhaustion.
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }
}

void protocol_block_in_31800::do_pause(channel_t) NOEXCEPT
{
    pause_performance();
}

void protocol_block_in_31800::do_resume(channel_t) NOEXCEPT
{
    if (!is_idle())
        start_performance();
}

void protocol_block_in_31800::do_purge(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map_->empty())
    {
        map_->clear();
        stop(error::sacrificed_channel);
    }
}

void protocol_block_in_31800::do_split(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    LOGP("Divide work (" << map_->size() << ") from [" << authority() << "].");

    restore(split(map_));
    restore(map_);
    map_ = std::make_shared<database::associations>();
    stop(error::sacrificed_channel);
}

map_ptr protocol_block_in_31800::split(const map_ptr& map) NOEXCEPT
{
    // Merge half of map into new half.
    const auto count = map->size();
    const auto half = std::make_shared<database::associations>();
    auto& index = map->get<database::association::pos>();
    const auto end = std::next(index.begin(), to_half(count));
    half->merge(index, index.begin(), end);
    return half;
}

// request hashes
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

    if (is_idle())
    {
        const auto message = create_get_data((map_ = map));
        SEND(message, handle_send, _1);
        return;
    }

    // There are two populated maps, return the new and leave the old alone.
    restore(map);
}

get_data protocol_block_in_31800::create_get_data(
    const map_ptr& map) const NOEXCEPT
{
    // clang has emplace_back bug (no matching constructor).
    // bip144: get_data uses witness constant but inventory does not.

    get_data getter{};
    getter.items.reserve(map->size());
    std::for_each(map->pos_begin(), map->pos_end(),
        [&](const database::association& item) NOEXCEPT
        {
            getter.items.push_back({ block_type_, item.hash });
        });

    return getter;
}

// check block
// ----------------------------------------------------------------------------

// TODO: Reduce to is_malleable_duplicate() under checkpoint/milestone.
code protocol_block_in_31800::check(const chain::block& block,
    const chain::context& ctx) const NOEXCEPT
{
    code ec{};
    return ec = block.check() ? ec : block.check(ctx);
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

    // TODO: is_malleated depends on an efficient query.is_malleable query.
    if (query.is_malleated(block))
    {
        // Disallow known malleated block, drop peer and keep trying.
        LOGR("Malleated block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        stop(error::malleated_block);
        return false;
    }

    const auto& link = it->link;
    const auto& ctx = it->context;

    // Check block.
    // ------------------------------------------------------------------------

    if (const auto error = check(block, ctx))
    {
        // Both duplicate and coincident malleability are possible here.
        if (block.has_duplicates() || block.is_malleable())
        {
            // Block has not been associated, so just continue with hashes.
        }
        else
        {
            if (!query.set_block_unconfirmable(link))
            {
                stop(node::error::store_integrity);
                return false;
            }

            notify(error::success, chase::unchecked, link);
            fire(events::block_unconfirmable, ctx.height);
        }

        LOGR("Unchecked block [" << encode_hash(hash) << ":" << ctx.height
            << "] from [" << authority() << "] " << error.message());

        stop(error);
        return false;
    }

    // Commit block.txs.
    // ------------------------------------------------------------------------

    // TODO: archive is_malleable for efficient query.is_malleable tests.
    if (query.set_link(*block.transactions_ptr(), link).is_terminal())
    {
        LOGF("Failure storing block [" << encode_hash(hash) << ":" << ctx.height
            << "] from [" << authority() << "].");
        stop(node::error::store_integrity);
        return false;
    }

    // ------------------------------------------------------------------------

    LOGP("Downloaded block [" << encode_hash(hash) << ":" << ctx.height
        << "] from [" << authority() << "].");

    notify(error::success, chase::checked, ctx.height);
    fire(events::block_archived, ctx.height);

    count(message->cached_size);
    map_->erase(it);
    if (is_idle())
    {
        LOGP("Getting more block hashes for [" << authority() << "].");
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }

    return true;
}

// get/put hashes
// ----------------------------------------------------------------------------

void protocol_block_in_31800::restore(const map_ptr& map) NOEXCEPT
{
    if (!map->empty())
        put_hashes(map, BIND(handle_put_hashes, _1));
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
    BC_ASSERT_MSG(map->size() <= max_inventory, "inventory overflow");

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
        notify(error::success, chase::starved, identifier());
        return;
    }

    POST(send_get_data, map);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
