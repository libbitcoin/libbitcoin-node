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
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>

#include <algorithm>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

// This class does NOT inherit from protocol_block_in_106.
#define CLASS protocol_block_in_31800

using namespace system;
using namespace database;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));

    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);
    protocol::start();
}

// overridden to add non-current node start.
void protocol_block_in_31800::subscribed(const code& ec, object_key) NOEXCEPT
{
    BC_ASSERT(stranded());

    // stopped() is true before stopping() is called (by base).
    if (stopped(ec))
    {
        unsubscribe_events();
        return;
    }

    // Start performance timing and download cycles if candidates are current.
    // This prevents a startup delay in which the node waits on a header.
    if (is_current(false))
    {
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2, _3));
    }
}

// overridden to add map handling (and stop_performance).
void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    restore(map_);
    map_ = chaser_check::empty_map();
    stop_performance();
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (download, split)
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::is_idle() const NOEXCEPT
{
    return map_->empty();
}

bool protocol_block_in_31800::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::split:
        {
            // chase::split is posted by notify_one() using subscription key.
            // 'value' is the channel that requested a split to obtain work.
            POST(do_split, channel_t{});
            break;
        }
        case chase::stall:
        {
            // If this channel has divisible work, split it and stop.
            // There are no channels reporting work, either stalled or done.
            // This is initiated by any channel notifying chase::starved.
            if (map_->size() > one)
            {
                POST(do_split, channel_t{});
            }

            break;
        }
        case chase::purge:
        {
            // If have work clear it and stop.
            // This is initiated by chase::regressed/disorganized.
            if (map_->size() > one)
            {
                POST(do_purge, channel_t{});
            }

            break;
        }
        case chase::download:
        {
            // There are count blocks to download at/above given header.
            // chase::headers is only sent for current candidate chain, and this
            // chase::download is only sent as a consequence of chase::headers.
            BC_ASSERT(std::holds_alternative<count_t>(value));
            POST(do_get_downloads, std::get<count_t>(value));
            break;
        }
        case chase::report:
        {
            BC_ASSERT(std::holds_alternative<count_t>(value));
            POST(do_report, std::get<count_t>(value));
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
        get_hashes(BIND(handle_get_hashes, _1, _2, _3));
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

void protocol_block_in_31800::do_report(count_t sequence) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Uses application logging since it outputs to a runtime option.
    LOGA("Work report [" << sequence << "] is (" << map_->size() << ") for ["
        << authority() << "].");
}

// request hashes
// ----------------------------------------------------------------------------

void protocol_block_in_31800::send_get_data(const map_ptr& map,
    const job::ptr& job) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
    {
        restore(map);
        return;
    }

    if (map->empty())
        return;

    // There are two populated maps, return new and leave old in place.
    if (!is_idle())
    {
        restore(map);
        return;
    }

    job_ = job;
    map_ = map;
    SEND(create_get_data(*map_), handle_send, _1);
}

get_data protocol_block_in_31800::create_get_data(
    const associations& map) const NOEXCEPT
{
    get_data data{};
    data.items.reserve(map.size());

    // bip144: get_data uses witness type_id but inv does not.
    std::for_each(map.pos_begin(), map.pos_end(), [&](const auto& item) NOEXCEPT
    {
        data.items.emplace_back(block_type_, item.hash);
    });

    return data;
}

// check block
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Preconditions.
    // ........................................................................

    const auto& block = message->block_ptr;
    const auto& hash = block->get_hash();
    const auto it = map_->find(hash);
    auto& query = archive();

    if (it == map_->end())
    {
        // Allow unrequested block, not counted toward performance.
        LOGR("Unrequested block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        return true;
    }

    const auto link = it->link;
    const auto height = it->context.height;

    // Check block.
    // ........................................................................

    const auto checked = is_under_checkpoint(height);
    const auto bypass = checked || query.is_milestone(link);

    // Tx commitments and malleation are checked under bypass. Invalidity is
    // only stored when a strong header has been stored, later to be found out
    // as invalid and not malleable. Stored invalidity prevents repeat
    // processing of the same invalid chain but is not necessary or desirable.
    if (const auto code = check(*block, it->context, bypass))
    {
        if (code == system::error::invalid_transaction_commitment ||
            code == system::error::invalid_witness_commitment)
        {
            LOGR("Malleated block [" << encode_hash(hash) << ":" << height
                << "] from [" << authority() << "] " << code.message()
                << " txs(" << block->transactions() << ")"
                << " segregated(" << block->is_segregated() << ").");
            stop(code);
            return false;
        }

        if (!query.set_block_unconfirmable(link))
        {
            stop(fault(error::protocol1));
            return false;
        }

        LOGR("Block failed check [" << encode_hash(hash) << ":" << height
            << "] from [" << authority() << "] " << code.message());
        notify(error::success, chase::unchecked, link);
        fire(events::block_unconfirmable, height);
        stop(code);
        return false;
    }

    // Commit block.txs.
    // ........................................................................

    if (const auto code = query.set_code(*block, link, checked))
    {
        LOGF("Failure storing block [" << encode_hash(hash) << ":" << height
            << "] from [" << authority() << "] " << code.message());

        stop(fault(code));
        return false;
    }

    // Advance.
    // ........................................................................

    LOGP("Downloaded block [" << encode_hash(hash) << ":" << height
        << "] from [" << authority() << "].");

    notify(ec, chase::checked, height);
    fire(events::block_archived, height);

    count(block->serialized_size(true));
    map_->erase(it);
    if (is_idle())
    {
        job_.reset();
        get_hashes(BIND(handle_get_hashes, _1, _2, _3));
    }

    return true;
}

// Identity is correct unless error::invalid_witness_commitment or
// error::invalid_transaction_commitment is returned. Only identity is required
// under bypass. Header state is checked by organize. 
code protocol_block_in_31800::check(const chain::block& block,
    const chain::context& ctx, bool bypass) const NOEXCEPT
{
    code ec{};
    if (bypass)
    {
        if (((ec = block.identify())) || ((ec = block.identify(ctx))))
            return ec;
    }
    else
    {
        if (((ec = block.check())) || ((ec = block.check(ctx))))
            return ec;
    }

    return error::success;
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
    const map_ptr& map, const job::ptr& job) NOEXCEPT
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
        notify(error::success, chase::starved, events_key());
        return;
    }

    POST(send_get_data, map, job);
}

// checkpoint
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::is_under_checkpoint(size_t height) const NOEXCEPT
{
    return height <= top_checkpoint_height_;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
