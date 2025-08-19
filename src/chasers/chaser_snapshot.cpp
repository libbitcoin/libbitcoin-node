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
#include <bitcoin/node/chasers/chaser_snapshot.hpp>

#include <atomic>
#include <chrono>
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_snapshot

using namespace system;
using namespace network;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_snapshot::chaser_snapshot(full_node& node) NOEXCEPT
  : chaser(node)
    ////snapshot_bytes_(node.config().node.snapshot_bytes),
    ////snapshot_valid_(node.config().node.snapshot_valid),
    ////snapshot_confirm_(node.config().node.snapshot_confirm),
    ////enabled_bytes_(to_bool(snapshot_bytes_)),
    ////enabled_valid_(to_bool(snapshot_valid_)),
    ////enabled_confirm_(to_bool(snapshot_confirm_))
{
}

// start
// ----------------------------------------------------------------------------

code chaser_snapshot::start() NOEXCEPT
{
    // Initial values assume all stops or starts are snapped.
    // get_top_validated is an expensive scan.

    ////if (enabled_bytes_)
    ////    bytes_ = archive().store_body_size();
    ////
    ////if (enabled_valid_)
    ////    valid_ = std::max(archive().get_top_confirmed(), checkpoint());
    ////    ////valid_ = std::max(archive().get_top_validated(), checkpoint());
    ////
    ////if (enabled_confirm_)
    ////    confirm_ = std::max(archive().get_top_confirmed(), checkpoint());

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_snapshot::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    if (suspended())
        return true;

    switch (event_)
    {
        // blocks first and headers first (checked) messages
        ////case chase::blocks:
        ////case chase::checked:
        ////{
        ////    if (!enabled_bytes_ || ec)
        ////        break;
        ////
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////    POST(do_archive, std::get<height_t>(value));
        ////    break;
        ////}
        ////case chase::valid:
        ////{
        ////    if (!enabled_valid_ || ec)
        ////        break;
        ////
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////    POST(do_valid, std::get<height_t>(value));
        ////    break;
        ////}
        ////case chase::confirmable:
        ////{
        ////    if (!enabled_confirm_ || ec)
        ////        break;
        ////
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////    POST(do_confirm, std::get<height_t>(value));
        ////    break;
        ////}
        case chase::block:
        {
            if (pruned_.load(std::memory_order_relaxed))
                break;

            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_prune, std::get<header_t>(value));
            break;
        }
        case chase::snap:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_snap, std::get<height_t>(value));
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// events
// ----------------------------------------------------------------------------

void chaser_snapshot::do_prune(header_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (pruned_.load(std::memory_order_relaxed) ||
        closed() || !archive().is_coalesced())
        return;

    const auto running = !suspended();
    LOG_ONLY(const auto start = logger::now();)

    if (const auto ec = prune([this](auto LOG_ONLY(event_),
        auto LOG_ONLY(table)) NOEXCEPT
    {
        // prune internally logs the prune event, otherwise is a snapshot.
        LOGN("snapshot::" << full_node::store::events.at(event_)
            << "(" << full_node::store::tables.at(table) << ")");
    }))
    {
        // Prune may fail due to chain not being coalesced following suspend,
        // in which case pruning will be attempted on announce until success.
        LOGF("Prune deferred, " << ec.message());
    }
    else
    {
        // Could become full before prune start (and it could still succeed).
        if (running && !archive().is_full())
            resume();

        pruned_.store(true, std::memory_order_relaxed);
        LOG_ONLY(const auto time = logger::now() - start;)
        LOG_ONLY(const auto span = duration_cast<milliseconds>(time);)
        LOGN("Pruned prevout cache at height [" << archive().get_top_confirmed()
            << "] complete in " << span.count() << " msecs.");
    }
}

void chaser_snapshot::do_snap(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    LOGN("Snapshot at recent height [" << height << "] is started.");
    take_snapshot(height);
}

////void chaser_snapshot::do_archive(size_t height) NOEXCEPT
////{
////    BC_ASSERT(stranded());
////    if (closed() || !update_bytes())
////        return;
////
////    LOGN("Snapshot at archived height [" << height << "] is started.");
////    take_snapshot(height);
////}
////
////void chaser_snapshot::do_valid(size_t height) NOEXCEPT
////{
////    BC_ASSERT(stranded());
////    if (closed() || !update_valid(height))
////        return;
////
////    LOGN("Snapshot at validated height [" << height << "] is started.");
////    take_snapshot(height);
////}
//// 
////void chaser_snapshot::do_confirm(size_t height) NOEXCEPT
////{
////    BC_ASSERT(stranded());
////    if (closed() || !update_confirm(height))
////        return;
////
////    LOGN("Snapshot at confirmable height [" << height << "] is started.");
////    take_snapshot(height);
////}

// utility
// ----------------------------------------------------------------------------

// Doesn't currently require strand.
void chaser_snapshot::take_snapshot(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto running = !suspended();
    LOG_ONLY(const auto start = logger::now();)

    if (const auto ec = snapshot([this](auto LOG_ONLY(event_),
        auto LOG_ONLY(table)) NOEXCEPT
    {
        LOGN("snapshot::" << full_node::store::events.at(event_)
            << "(" << full_node::store::tables.at(table) << ")");
    }))
    {
        LOGF("Snapshot at height [" << height << "] failed, " << ec.message());
    }
    else
    {
        // Could become full before snapshot start (and it could still succeed).
        if (running && !archive().is_full())
            resume();

        LOG_ONLY(const auto time = logger::now() - start;)
        LOG_ONLY(const auto span = duration_cast<seconds>(time);)
        LOGN("Snapshot at height [" << height << "] complete in "
            << span.count() << " secs.");
    }

    ////// Current values may have raced ahead but this is sufficient.
    ////// Snapshot failure also resets these values, to prevent cycling.
    ////const auto& query = archive();
    ////
    ////if (enabled_bytes_)
    ////    bytes_ = query.store_body_size();
    ////
    ////if (enabled_valid_)
    ////    valid_ = height;
    ////    ////valid_ = std::max(query.get_top_validated(), checkpoint());
    ////
    ////if (enabled_confirm_)
    ////    valid_ = std::max(query.get_top_confirmed(), checkpoint());
}

////bool chaser_snapshot::update_bytes() NOEXCEPT
////{
////    // This is the most costly, sizing for every download, but is just a sum.
////    // Wire size might be better, but there is no constant time query for it.
////    const auto growth = floored_subtract(archive().store_body_size(), bytes_);
////    return growth >= snapshot_bytes_;
////}
////
////bool chaser_snapshot::update_valid(height_t height) NOEXCEPT
////{
////    // The difference may have been negative and therefore show zero growth.
////    const auto growth = floored_subtract(height, valid_);
////    return growth >= snapshot_valid_;
////}
////
////bool chaser_snapshot::update_confirm(height_t height) NOEXCEPT
////{
////    // The difference may have been negative and therefore show zero growth.
////    const auto growth = floored_subtract(height, valid_);
////    return growth >= snapshot_confirm_;
////}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
