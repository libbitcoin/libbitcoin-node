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
#include <bitcoin/node/chasers/chaser_check.hpp>

#include <algorithm>
#include <memory>
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

// start/stop
// ----------------------------------------------------------------------------

code chaser_check::start() NOEXCEPT
{
    start_tracking();
    set_position(archive().get_fork());
    requested_ = position();
    const auto added = set_unassociated();
    LOGN("Fork point (" << requested_ << ") unassociated (" << added << ").");

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

void chaser_check::stopping(const code& ec) NOEXCEPT
{
    // Allow job completion as soon as all protocols are closed.
    stop_tracking();
    chaser::stopping(ec);
}

bool chaser_check::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // TODO: allow required messages.
    ////// Stop generating query during suspension.
    ////if (suspended())
    ////    return true;

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
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_checked, std::get<height_t>(value));
            break;
        }
        case chase::regressed:
        case chase::disorganized:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_regressed, std::get<height_t>(value));
            break;
        }
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case chase::headers:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_headers, std::get<height_t>(value));
            break;
        }
        case chase::confirmable:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_confirmable, std::get<height_t>(value));
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

void chaser_check::start_tracking() NOEXCEPT
{
    // Called from start.
    ////BC_ASSERT(stranded());

    job_ = std::make_shared<job>(BIND(handle_purged, _1));
}

void chaser_check::stop_tracking() NOEXCEPT
{
    // Called by stop (node thread).
    ////BC_ASSERT(stranded());

    // shared_ptr.reset() is thread safe so can be called at stop.
    // Resetting our own pointer allows destruct and call to handle_purged.
    job_.reset();
}

void chaser_check::handle_purged(const code& ec) NOEXCEPT
{
    if (closed())
        return;

    boost::asio::post(strand(),
        std::bind(&chaser_check::do_handle_purged,
            this, ec));
}

void chaser_check::do_handle_purged(const code&) NOEXCEPT
{
    BC_ASSERT(stranded());

    start_tracking();
    do_bump(height_t{});
}

// regression
// ----------------------------------------------------------------------------

void chaser_check::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Inconsequential regression, work isn't there yet.
    if (branch_point >= position())
        return;

    // Update position, purge outstanding work, and wait on track completion.
    set_position(branch_point);
    stop_tracking();
    maps_.clear();
    notify(error::success, chase::purge, branch_point);
}

// track downloaded in order (to move download window)
// ----------------------------------------------------------------------------

void chaser_check::do_confirmable(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Confirmations are ordered and notification order is guaranteed.
    confirmed_ = height;

    // The full set of requested hashes has been confirmed.
    if (confirmed_ == requested_)
        do_headers(height);
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
    auto height = position();

    // TODO: query.is_associated() is expensive (hashmap search).
    // Skip checked blocks starting immediately after last checked.
    while (!closed() &&
        query.is_associated(query.to_candidate((height = add1(height)))))
    {
        set_position(height);
    }

    do_headers(sub1(height));
}

// add headers
// ----------------------------------------------------------------------------

void chaser_check::do_headers(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto added = set_unassociated();
    if (!is_zero(added))
        notify(error::success, chase::download, added);
}

// get/put hashes
// ----------------------------------------------------------------------------

bool chaser_check::purging() const NOEXCEPT
{
    return !job_;
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
    handler(error::success, map, job_);
}

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
    // Called from start.
    ////BC_ASSERT(stranded());
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

    // Defer new work issuance until gaps filled and confirmation caught up.
    if (position() < requested_ ||
        confirmed_ < requested_)
        return {};

    // Inventory size gets set only once.
    if (is_zero(inventory_))
        if (is_zero((inventory_ = get_inventory_size())))
            return zero;

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
    const size_t peers = config().network.outbound_connections;
    if (is_zero(peers) || !is_current())
        return zero;

    const auto& query = archive();
    const auto fork = query.get_fork();

    const auto span = system::ceilinged_multiply(messages::max_inventory, peers);
    const auto step = std::min(maximum_concurrency_, span);
    const auto inventory = query.get_unassociated_count_above(fork, step);
    return system::ceilinged_divide(inventory, peers);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
