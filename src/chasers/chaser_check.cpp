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
    maximum_concurrency_(node.config().node.maximum_concurrency_()),
    maximum_height_(node.config().node.maximum_height_()),
    connections_(node.config().network.outbound_connections)
{
}

// static
map_ptr chaser_check::empty_map() NOEXCEPT
{
    return std::make_shared<associations>();
}

// static
map_ptr chaser_check::split(const map_ptr& map) NOEXCEPT
{
    const auto half = empty_map();
    auto& index = map->get<association::pos>();
    const auto end = std::next(index.begin(), to_half(map->size()));
    half->merge(index, index.begin(), end);
    return half;
}

// start
// ----------------------------------------------------------------------------
code chaser_check::start() NOEXCEPT
{
    requested_ = validated_ = archive().get_fork();
    const auto add = get_unassociated();
    LOGN("Fork point (" << validated_ << ") unassociated (" << add << ").");

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_check::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::header:
        {
            POST(do_header, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::preconfirmable:
        {
            POST(do_preconfirmable, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::regressed:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_disorganized, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::malleated:
        {
            POST(do_malleated, possible_narrow_cast<header_t>(value));
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

// add headers
// ----------------------------------------------------------------------------

void chaser_check::do_header(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    const auto add = get_unassociated();
    if (!is_zero(add))
        notify(error::success, chase::download, add);
}

// set floor
// ----------------------------------------------------------------------------

void chaser_check::do_preconfirmable(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    validated_ = height;
    do_header(height);
}

// purge headers
// ----------------------------------------------------------------------------

void chaser_check::do_regressed(height_t branch_point) NOEXCEPT
{
    // If branch point is at or above last validated there is nothing to do.
    if (branch_point < validated_)
        validated_ = branch_point;

    maps_.clear();
    notify(error::success, chase::purge, branch_point);
}

// purge headers
// ----------------------------------------------------------------------------

void chaser_check::do_disorganized(height_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Revert to confirmed top as the candidate chain is fully reverted.
    validated_ = top;

    maps_.clear();
    notify(error::success, chase::purge, top);
}

// re-download malleated block (invalid but malleable)
// ----------------------------------------------------------------------------

// The archived malleable block was found to be invalid (treat as malleated).
// The block/header hash cannot be marked unconfirmable due to malleability, so
// disassociate the block and then add the block hash back to the current set.
void chaser_check::do_malleated(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    association out{};
    if (!query.set_dissasociated(link))
    {
        suspend(query.get_code());
        return;
    }

    if (!query.get_unassociated(out, link))
    {
        suspend(error::store_integrity);
        return;
    }

    maps_.push_back(std::make_shared<associations>(associations{ out }));
    notify(error::success, chase::download, one);
}

// get/put hashes
// ----------------------------------------------------------------------------

void chaser_check::get_hashes(map_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    boost::asio::post(strand(),
        std::bind(&chaser_check::do_get_hashes,
            this, std::move(handler)));
}

void chaser_check::put_hashes(const map_ptr& map,
    result_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    boost::asio::post(strand(),
        std::bind(&chaser_check::do_put_hashes,
            this, map, std::move(handler)));
}

////LOGN("Hashes -" << map->size() << " (" << count_maps() << ") remain.");
void chaser_check::do_get_hashes(const map_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    const auto map = get_map();
    handler(error::success, map);
}

////LOGN("Hashes +" << map->size() << " (" << count_maps() << ") remain.");
void chaser_check::do_put_hashes(const map_ptr& map,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(map->size() <= messages::max_inventory);
    BC_ASSERT(stranded());

    if (closed())
        return;

    if (!map->empty())
    {
        maps_.push_back(map);
        notify(error::success, chase::download, map->size());
    }

    handler(error::success);
}

// utilities
// ----------------------------------------------------------------------------

map_ptr chaser_check::get_map() NOEXCEPT
{
    BC_ASSERT(stranded());

    return maps_.empty() ? empty_map() : pop_front(maps_);
}

// Get all unassociated block records from start to stop heights.
// Groups records into table sets by inventory set size, limited by advance.
// Return the total number of records obtained and set requested_ to last.
size_t chaser_check::get_unassociated() NOEXCEPT
{
    // Called from start.
    ////BC_ASSERT(stranded());

    size_t count{};
    if (validated_ < requested_)
        return count;

    // Inventory size gets set only once.
    if (is_zero(inventory_))
    {
        inventory_ = get_maximum_inventory();
        if (is_zero(inventory_)) return zero;
    }

    // Due to previous downloads, validation can race ahead of last request.
    // The last request (requested_) stops at the last gap in the window, but
    // validation continues until the next gap. Start next scan above validated
    // not last requested, since all between are already downloaded.
    const auto& query = archive();
    const auto requested = requested_;
    const auto step = ceilinged_add(validated_, maximum_concurrency_);
    const auto stop = std::min(step, maximum_height_);

    while (true)
    {
        const auto map = std::make_shared<associations>(
            query.get_unassociated_above(requested_, inventory_, stop));

        if (map->empty())
            break;

        BC_ASSERT(map->size() <= messages::max_inventory);
        maps_.push_back(map);
        requested_ = map->top().height;
        count += map->size();
    }

    LOGN("Advance by ("
        << maximum_concurrency_ << ") above ("
        << requested << ") from ("
        << validated_ << ") stop ("
        << stop << ") found ("
        << count << ") last ("
        << requested_ << ").");

    return count;
}

size_t chaser_check::get_maximum_inventory() const NOEXCEPT
{
    // Either condition means blocks shouldn't be getting downloaded (yet).
    const auto peers = config().network.outbound_connections;
    if (is_zero(peers) || !is_current())
        return zero;

    const auto step = config().node.maximum_concurrency_();
    const auto fork = archive().get_fork();
    const auto scan = archive().get_unassociated_count_above(fork, step);
    const auto chunk = std::min(step, scan);
    const auto inventory = std::min(chunk, messages::max_inventory);
    return system::ceilinged_divide(inventory, peers);
}

////size_t chaser_check::count_maps() const NOEXCEPT
////{
////    return std::accumulate(maps_.begin(), maps_.end(), zero,
////        [](size_t sum, const map_ptr& map) NOEXCEPT
////        {
////            return sum + map->size();
////        });
////}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
