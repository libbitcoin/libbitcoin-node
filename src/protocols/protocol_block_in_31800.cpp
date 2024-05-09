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
#include <bitcoin/node/chasers/chasers.hpp>
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

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3),
        BIND(complete_event, _1, _2));

    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);
    protocol::start();
}

// protected
void protocol_block_in_31800::complete_event(const code& ec,
    object_key key) NOEXCEPT
{
    POST(do_complete_event, ec, key);
}

// private
void protocol_block_in_31800::do_complete_event(const code&,
    object_key key) NOEXCEPT
{
    BC_ASSERT(stranded());
    key_ = key;

    // stopped() is true before stopping() is called (by base).
    if (stopped())
    {
        unsubscribe_events(key_);
        return;
    }

    // Start performance timing and download cycles if candidates are current.
    // This prevents a startup delay in which the node waits on a header.
    if (is_current())
    {
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }
}

// If this is invoked before do_complete_event then it will unsubscribe.
void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    restore(map_);
    map_ = chaser_check::empty_map();
    stop_performance();
    unsubscribe_events(key_);
    protocol::stopping(ec);
}

// handle events (download, split)
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::is_idle() const NOEXCEPT
{
    return map_->empty();
}

bool protocol_block_in_31800::handle_event(const code&,
    chase event_, event_value value) NOEXCEPT
{
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::split:
        {
            // TODO: remove condition once notify_one is used for chase::split.
            // If value identifies this channel (slowest), split work and stop.
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
            // If have work clear it and stop.
            // This is initiated by chase::regressed/disorganized.
            if (map_->size() >= minimum_for_stall_divide)
            {
                POST(do_purge, channel_t{});
            }

            break;
        }
        case chase::download:
        {
            // There are count blocks to download at/above given header.
            // chase::header is only sent for current candidate chain, and this
            // chase::download is only sent as a consequence of chase::header.
            POST(do_get_downloads, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::report:
        {
            POST(do_report, possible_narrow_cast<count_t>(value));
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
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

void protocol_block_in_31800::do_purge(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map_->empty())
    {
        LOGV("Purge work (" << map_->size() << ") from [" << authority() << "].");
        map_->clear();
        stop(error::sacrificed_channel);
    }
}

void protocol_block_in_31800::do_split(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    LOGV("Divide work (" << map_->size() << ") from [" << authority() << "].");
    restore(chaser_check::split(map_));
    restore(map_);
    map_ = chaser_check::empty_map();
    stop(error::sacrificed_channel);
}

void protocol_block_in_31800::do_report(count_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Uses application logging since it outputs to a runtime option.
    LOGA("Work (" << map_->size() << ") for channel [" << authority() << "].");
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
    get_data getter{};
    getter.items.reserve(map->size());

    // bip144: get_data uses witness constant but inventory does not.
    // clang emplace_back bug (no matching constructor), using push_back.
    std::for_each(map->pos_begin(), map->pos_end(),
        [&](const database::association& item) NOEXCEPT
        {
            getter.items.push_back({ block_type_, item.hash });
        });

    return getter;
}

// check block
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Preconditions (requested and not malleated).
    // ........................................................................

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

    if (query.is_malleated(block))
    {
        // Disallow known block malleation, drop peer and keep trying.
        LOGR("Malleated block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        stop(error::malleated_block);
        return false;
    }

    const auto& link = it->link;
    const auto& ctx = it->context;

    // Check block.
    // ........................................................................

    if (const auto code = check(block, ctx))
    {
        // Both forms of malleabilty are possible here.
        // Malleable has not been associated, so just drop peer and continue.
        if (!block.is_malleable())
        {
            if (!query.set_block_unconfirmable(link))
            {
                suspend(query.get_code());
                return false;
            }

            notify(error::success, chase::unchecked, link);
            fire(events::block_unconfirmable, ctx.height);
        }

        LOGR("Unchecked block [" << encode_hash(hash) << ":" << ctx.height
            << "] from [" << authority() << "] " << code.message());

        stop(code);
        return false;
    }

    // Commit block.txs.
    // ........................................................................

    if (query.set_link(*block.transactions_ptr(), link,
        block.serialized_size(true)).is_terminal())
    {
        LOGV("Failure storing block [" << encode_hash(hash) << ":"
            << ctx.height << "] from [" << authority() << "].");

        stop(suspend(query.get_code()));
        return false;
    }

    // Advance.
    // ........................................................................

    LOGP("Downloaded block [" << encode_hash(hash) << ":" << ctx.height
        << "] from [" << authority() << "].");

    notify(error::success, chase::checked, ctx.height);
    fire(events::block_archived, ctx.height);

    count(message->cached_size);
    map_->erase(it);
    if (is_idle())
        get_hashes(BIND(handle_get_hashes, _1, _2));

    return true;
}

// Transaction commitments are required under checkpoint/milestone, and other
// checks are comparable to the bypass condition cost, so just do them.
code protocol_block_in_31800::check(const chain::block& block,
    const chain::context& ctx) const NOEXCEPT
{
    code ec{};
    return ec = block.check() ? ec : block.check(ctx);
}

// get/put hashes
// ----------------------------------------------------------------------------

void protocol_block_in_31800::restore(const map_ptr& map) NOEXCEPT
{
    if (!map->empty())
        put_hashes(map, BIND(handle_put_hashes, _1, map->size()));
}

void protocol_block_in_31800::handle_put_hashes(const code& ec,
    size_t count) NOEXCEPT
{
    LOGV("Put (" << count << ") work for [" << authority() << "].");

    if (ec)
    {
        LOGF("Error putting work for [" << authority() << "] " << ec.message());
    }
}

void protocol_block_in_31800::handle_get_hashes(const code& ec,
    const map_ptr& map) NOEXCEPT
{
    LOGV("Got (" << map->size() << ") work for [" << authority() << "].");

    if (stopped())
    {
        restore(map);
        return;
    }

    if (ec)
    {
        LOGF("Error getting work for [" << authority() << "] " << ec.message());
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
