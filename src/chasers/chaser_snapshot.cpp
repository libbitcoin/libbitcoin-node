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
#include <bitcoin/node/chasers/chaser_snapshot.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_snapshot

using namespace system;
using namespace network;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_snapshot::chaser_snapshot(full_node& node) NOEXCEPT
  : chaser(node),
    snapshot_bytes_(node.config().node.snapshot_bytes),
    snapshot_valid_(node.config().node.snapshot_valid),
    enabled_bytes_(to_bool(snapshot_bytes_)),
    enabled_valid_(to_bool(snapshot_valid_))
{
}

// start
// ----------------------------------------------------------------------------

code chaser_snapshot::start() NOEXCEPT
{
    // Initial values assume all stops or starts are snapped.

    if (enabled_bytes_)
        bytes_ = archive().store_body_size();

    if (enabled_valid_)
        valid_ = std::max(archive().get_top_confirmed(), top_checkpoint());

    if (enabled_bytes_ || enabled_valid_)
    {
        SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    }

    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_snapshot::handle_event(const code& ec, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::block:
        case chase::checked:
        {
            if (!enabled_bytes_ || ec) break;

            // Checked blocks are our of order, so this is probalistic.
            POST(do_archive, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::confirmable:
        {
            // Skip bypassed confirmable events as they are close to archive.
            if (!enabled_valid_ || ec) break;

            // Confirmable covers all validation except set_confirmed (link).
            POST(do_confirm, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::snapshot:
        {
            // Inherently disabled if both types are disabled.
            // error::disk_full (infrequent, compute height).
            POST(do_full, archive().get_top_confirmed());
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

void chaser_snapshot::do_archive(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || !update_bytes())
        return;

    LOGN("Snapshot at archived height [" << height << "] is started.");
    do_snapshot(height);
}
 
void chaser_snapshot::do_confirm(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || !update_valid(height))
        return;

    LOGN("Snapshot at confirmable height [" << height << "] is started.");
    do_snapshot(height);
}

void chaser_snapshot::do_full(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || is_redundant(height))
        return;

    LOGN("Snapshot at disk full height [" << height << "] is started.");
    do_snapshot(height);
}

// utility
// ----------------------------------------------------------------------------

void chaser_snapshot::do_snapshot(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto start = logger::now();
    const auto ec = snapshot([this](auto event_, auto table) NOEXCEPT
    {
        LOGN("snapshot::" << full_node::store::events.at(event_)
            << "(" << full_node::store::tables.at(table) << ")");
    });

    if (ec)
    {
        // Does not suspend node, will keep downloading until full.
        LOGN("Snapshot at height [" << height << "] failed with error '"
            << ec.message() << "'.");
    }
    else
    {
        const auto span = duration_cast<seconds>(logger::now() - start);
        LOGN("Snapshot at height [" << height << "] complete in "
            << span.count() << " secs.");
    }

    span(events::snapshot_span, start);
}

bool chaser_snapshot::update_bytes() NOEXCEPT
{
    // This is the most costly, sizing for every download, but is just a sum.
    // Wire size might be better, but there is no constant time query for it.
    const auto current = archive().store_body_size();
    const auto growth = floored_subtract(current, bytes_);
    const auto sufficient = (growth >= snapshot_bytes_);
    bytes_ = sufficient ? current : bytes_;
    return sufficient;
}

bool chaser_snapshot::update_valid(height_t height) NOEXCEPT
{
    // The difference may have been negative and therefore show zero growth.
    const auto growth = floored_subtract(height, valid_);
    const auto sufficient = (growth >= snapshot_valid_);
    valid_ = sufficient ? height : valid_;
    return sufficient;
}

bool chaser_snapshot::is_redundant(height_t height) const NOEXCEPT
{
    return valid_ == height || bytes_ == archive().store_body_size();
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
