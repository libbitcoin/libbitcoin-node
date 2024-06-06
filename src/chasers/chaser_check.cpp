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
    set_position(archive().get_fork());
    requested_ = position();
    const auto added = set_unassociated();
    LOGN("Fork point (" << requested_ << ") unassociated (" << added << ").");

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
        // Track downloaded.
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            POST(do_checked, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::regressed:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case chase::bypass:
        {
            POST(set_bypass, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::header:
        {
            POST(do_header, possible_narrow_cast<height_t>(value));
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

// track downloaded in order (to move download window)
// ----------------------------------------------------------------------------

void chaser_check::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Inconsequential regression, work isn't there yet.
    if (branch_point >= position())
        return;

    // Update position, purge outstanding work, and wait.
    set_position(branch_point);
    maps_.clear();
    purging_ = true;
    notify(error::success, chase::purge, branch_point);

    // TODO: wait on current branch purge completion, then do set_unstrong.
    // TODO: while waiting ignore new work, so when complete must bump to
    // TODO: avoid temporary stall.
}

void chaser_check::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Candidate block was checked at the given height, advance.
    if (height == add1(position()))
        do_bump(height);
}

void chaser_check::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (purging())
        return;

    const auto& query = archive();

    // TODO: query.is_associated() is expensive (hashmap search).
    // Skip checked blocks starting immediately after last checked.
    while (!closed() && query.is_associated(
        query.to_candidate(add1(position()))))
            set_position(add1(position()));

    set_unassociated();
}

// add headers
// ----------------------------------------------------------------------------

void chaser_check::do_header(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto added = set_unassociated();
    if (!is_zero(added))
        notify(error::success, chase::download, added);
}

// re-download malleated block (invalid but malleable)
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
        fault(error::set_dissasociated);
        return;
    }

    if (!query.get_unassociated(out, link))
    {
        fault(error::get_unassociated);
        return;
    }

    const auto map = std::make_shared<associations>(associations{ out });
    if (set_map(map) && !purging())
        notify(error::success, chase::download, one);
}

// get/put hashes
// ----------------------------------------------------------------------------

bool chaser_check::purging() const NOEXCEPT
{
    return purging_;
}

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

void chaser_check::do_get_hashes(const map_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed() || purging())
        return;

    const auto map = get_map();
    handler(error::success, map, bypass());
}

// It is possible that this call can be made before a purge has been sent and
// received after. This may result in unnecessary work and incorrect bypass.
void chaser_check::do_put_hashes(const map_ptr& map,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(map->size() <= messages::max_inventory);
    BC_ASSERT(stranded());
    if (closed() || purging())
        return;

    if (set_map(map))
        notify(error::success, chase::download, map->size());

    handler(error::success);
}

// utilities
// ----------------------------------------------------------------------------

map_ptr chaser_check::get_map() NOEXCEPT
{
    BC_ASSERT(stranded());

    return maps_.empty() ? empty_map() : pop_front(maps_);
}


bool chaser_check::set_map(const map_ptr& map) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT(map->size() <= messages::max_inventory);

    if (map->empty())
        return false;

    maps_.push_back(map);
    return true;
}

// Get all unassociated block records from start to stop heights.
// Groups records into table sets by inventory set size, limited by advance.
// Return the total number of records obtained and set requested_ to last.
size_t chaser_check::set_unassociated() NOEXCEPT
{
    // Called from start.
    ////BC_ASSERT(stranded());
    if (closed() || purging())
        return {};

    // Defer new work issuance until all gaps are filled.
    if (position() < requested_ || requested_ >= maximum_height_)
        return {};

    // Inventory size gets set only once.
    if (is_zero(inventory_))
    {
        inventory_ = get_inventory_size();
        if (is_zero(inventory_)) return zero;
    }

    // Due to previous downloads, validation can race ahead of last request.
    // The last request (requested_) stops at the last gap in the window, but
    // validation continues until the next gap. Start next scan above validated
    // not last requested, since all between are already downloaded.
    const auto& query = archive();
    const auto requested = requested_;
    const auto step = ceilinged_add(position(), maximum_concurrency_);
    const auto stop = std::min(step, maximum_height_);
    size_t count{};

    while (true)
    {
        // Calls query.is_associated() per block, expensive (hashmap search).
        const auto map = std::make_shared<associations>(
            query.get_unassociated_above(requested_, inventory_, stop));

        if (!set_map(map))
            break;

        requested_ = map->top().height;
        count += map->size();
    }

    LOGN("Advance by ("
        << maximum_concurrency_ << ") above ("
        << requested << ") from ("
        << position() << ") stop ("
        << stop << ") found ("
        << count << ") last ("
        << requested_ << ").");

    return count;
}

size_t chaser_check::get_inventory_size() const NOEXCEPT
{
    // Either condition means blocks shouldn't be getting downloaded (yet).
    const auto peers = config().network.outbound_connections;
    if (is_zero(peers) || !is_current())
        return zero;

    const auto step = config().node.maximum_concurrency_();
    const auto fork = archive().get_fork();
    const auto scan = archive().get_unassociated_count_above(fork, step);
    const auto span = std::min(step, scan);
    const auto inventory = std::min(span, messages::max_inventory);
    return system::ceilinged_divide(inventory, peers);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
