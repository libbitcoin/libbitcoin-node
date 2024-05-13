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
    snapshot_interval_(node.config().node.snapshot_interval_())
{
}

// start
// ----------------------------------------------------------------------------

// initialize snapshot tracking state.
code chaser_snapshot::start() NOEXCEPT
{
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

    switch (event_)
    {
        ////case chase::block:
        ////case chase::checked:
        case chase::confirmable:
        {
            // chase::confirmable posts height (organized posts link).
            POST(do_snapshot, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::snapshot:
        {
            // error::disk_full
            POST(do_snapshot, archive().get_top_confirmed());
            break;
        }
        case chase::stop:
        {
            // full_node.stop()
            POST(do_snapshot, archive().get_top_confirmed());
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

void chaser_snapshot::do_snapshot(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Height may decrease or increase, any difference is acceptable.
    if (height == current_ || closed())
        return;

    execute_snapshot((current_ = height));
}

void chaser_snapshot::execute_snapshot(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    LOGN("Snapshot at height [" << height << "] is started.");

    const auto start = wall_clock::now();
    const auto ec = snapshot([this](auto event_, auto table) NOEXCEPT
    {
        LOGN("snapshot::" << full_node::store::events.at(event_)
            << "(" << full_node::store::tables.at(table) << ")");
    });

    if (ec)
    {
        LOGN("Snapshot at height [" << height << "] failed with error, "
            << ec.message());
    }
    else
    {
        auto span = duration_cast<milliseconds>(wall_clock::now() - start);
        LOGN("Snapshot at height [" << height << "] complete in "
            << ec.message() << span.count());
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
