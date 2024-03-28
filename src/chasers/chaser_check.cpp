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
#include <bitcoin/node/chasers/chaser_check.hpp>

#include <algorithm>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_check

using namespace system;
using namespace system::chain;
using namespace network;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_check::chaser_check(full_node& node) NOEXCEPT
  : chaser(node),
    connections_(node.network_settings().outbound_connections),
    inventory_(system::lesser(node.config().node.maximum_inventory,
        network::messages::max_inventory))
{
}

// start
// ----------------------------------------------------------------------------

code chaser_check::start() NOEXCEPT
{
    const auto fork_point = archive().get_fork();
    const auto added = get_unassociated(maps_, fork_point);
    LOGN("Fork point (" << fork_point << ") unassociated (" << added << ").");

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_check::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    switch (event_)
    {
        case chase::header:
        {
            BC_ASSERT(std::holds_alternative<size_t>(value));
            POST(do_add_headers, std::get<size_t>(value));
            break;
        }
        case chase::disorganized:
        {
            BC_ASSERT(std::holds_alternative<size_t>(value));
            POST(do_purge_headers, std::get<size_t>(value));
            break;
        }
        ////case chase::header:
        case chase::download:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::pause:
        case chase::resume:
        case chase::bump:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmed:
        case chase::unpreconfirmed:
        case chase::confirmed:
        case chase::unconfirmed:
        ////case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

// add headers
// ----------------------------------------------------------------------------

void chaser_check::do_add_headers(size_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto added = get_unassociated(maps_, branch_point);

    ////LOGN("Branch point (" << branch_point << ") unassociated (" << added
    ////    << ").");

    if (is_zero(added))
        return;

    notify(error::success, chase::download, added);
}

// purge headers
// ----------------------------------------------------------------------------

void chaser_check::do_purge_headers(size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Candidate chain has been reset (from fork point) to confirmed top.
    // Since all blocks are confirmed through fork point, and all above are to
    // be purged, it simply means purge all hashes (reset all). All channels
    // will get the purge notification before any subsequent download notify.
    maps_.clear();
    notify(error::success, chase::purge, top);
}

// get/put hashes
// ----------------------------------------------------------------------------

void chaser_check::get_hashes(map_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_check::do_get_hashes,
            this, std::move(handler)));
}

void chaser_check::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_check::do_put_hashes,
            this, map, std::move(handler)));
}

void chaser_check::do_get_hashes(const map_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto map = get_map(maps_);

    ////LOGN("Hashes -" << map->size() << " ("
    ////    << count_map(maps_) << ") remain.");
    handler(error::success, map);
}

void chaser_check::do_put_hashes(const map_ptr& map,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map->empty())
    {
        maps_.push_back(map);
        notify(error::success, chase::download, map->size());
    }

    ////LOGN("Hashes +" << map->size() << " ("
    ////    << count_map(maps_) << ") remain.");
    handler(error::success);
}

// utilities
// ----------------------------------------------------------------------------

size_t chaser_check::get_unassociated(maps& table, size_t start) const NOEXCEPT
{
    size_t added{};
    while (true)
    {
        const auto map = make_map(start, inventory_);
        if (map->empty()) break;
        table.push_back(map);
        start = map->top().height;
        added += map->size();
    }

    return added;
}

size_t chaser_check::count_map(const maps& table) const NOEXCEPT
{
    return std::accumulate(table.begin(), table.end(), zero,
        [](size_t sum, const map_ptr& map) NOEXCEPT
        {
            return sum + map->size();
        });
}

map_ptr chaser_check::make_map(size_t start,
    size_t count) const NOEXCEPT
{
    return std::make_shared<database::associations>(
        archive().get_unassociated_above(start, count));
}

map_ptr chaser_check::get_map(maps& table) NOEXCEPT
{
    return table.empty() ? std::make_shared<database::associations>() :
        pop_front(table);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
