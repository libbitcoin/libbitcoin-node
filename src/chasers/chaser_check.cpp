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
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_check

using namespace system;
using namespace system::chain;
using namespace database;
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
        messages::max_inventory))
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
    using namespace system;
    switch (event_)
    {
        case chase::header:
        {
            POST(do_add_headers, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_purge_headers, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::malleated:
        {
            POST(do_malleated, possible_narrow_cast<header_t>(value));
            break;
        }
        case chase::stop:
        {
            // TODO: handle fault.
            break;
        }
        case chase::start:
        case chase::pause:
        case chase::resume:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::block:
        ////case chase::header:
        case chase::download:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmable:
        case chase::unpreconfirmable:
        case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        ////case chase::disorganized:
        ////case chase::malleated:
        case chase::transaction:
        case chase::template_:
        ////case chase::stop:
        {
            break;
        }
    }
}

// add headers
// ----------------------------------------------------------------------------

void chaser_check::do_add_headers(height_t branch_point) NOEXCEPT
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

void chaser_check::do_purge_headers(height_t top) NOEXCEPT
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
    result_handler&& handler) NOEXCEPT
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
    const result_handler& handler) NOEXCEPT
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

// Handle malleated (invalid but malleable) block.
// ----------------------------------------------------------------------------

// The archived malleable block instance was found to be invalid (malleated).
// The block/header hash cannot be marked unconfirmable due to malleability, so
// disassociate the block and then add the block hash back to the current set.
void chaser_check::do_malleated(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    association out{};
    if (!query.dissasociate(link) || !query.get_unassociated(out, link))
    {
        fault(error::store_integrity);
        return;
    }

    maps_.push_back(std::make_shared<associations>(associations{ out }));
    notify(error::success, chase::download, one);
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
    // Known malleated blocks are disassociated and therefore appear here.
    return std::make_shared<associations>(
        archive().get_unassociated_above(start, count));
}

map_ptr chaser_check::get_map(maps& table) NOEXCEPT
{
    return table.empty() ? std::make_shared<associations>() : pop_front(table);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
