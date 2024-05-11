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
        case chase::snapshot:
        {
            // Either from confirmed or disk full.
            POST(do_snapshot, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::stop:
        {
            // From full_node.stop().
            POST(do_snapshot, height_t{});
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

    if (closed())
        return;

    if (const auto ec = snapshot([&](auto event_, auto table) NOEXCEPT
    {
        LOGN("Snapshot at (" << height << ") event ["
            << static_cast<size_t>(event_) << ", "
            << static_cast<size_t>(table) << "].");
    }))
    {
        LOGN("Snapshot failed, " << ec.message());
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
