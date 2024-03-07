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
#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_check

using namespace network;
using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_check::chaser_check(full_node& node) NOEXCEPT
  : chaser(node),
    inventory_(system::lesser(node.node_settings().maximum_inventory,
        network::messages::max_inventory))
{
}

chaser_check::~chaser_check() NOEXCEPT
{
}

// start
// ----------------------------------------------------------------------------

code chaser_check::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());
    LOGN("Candidate fork (" << archive().get_fork() << ").");

    // Initialize map to all unassociated candidate blocks.
    map_table_.push_back(std::make_shared<database::associations>(
        archive().get_all_unassociated()));
    
    LOGN("Unassociated candidates (" << map_table_.at(0)->size() << ").");
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_check::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::strong)
    {
        BC_ASSERT(std::holds_alternative<chaser::height_t>(value));
        handle_strong(std::get<height_t>(value));
    }
}

// Stale branches are just be allowed to complete (still downloaded).
void chaser_check::handle_strong(height_t branch_point) NOEXCEPT
{
    const auto map = std::make_shared<database::associations>(
        archive().get_all_unassociated_above(branch_point));

    if (map->empty())
        return;

    network::result_handler handler = BIND(handle_put_hashes, _1);
    POST(do_put_hashes, map, std::move(handler));
}

void chaser_check::handle_put_hashes(const code&) NOEXCEPT
{
    BC_ASSERT(stranded());
}

// methods
// ----------------------------------------------------------------------------

void chaser_check::get_hashes(handler&& handler) NOEXCEPT
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

// TODO: post event causing channels to put some?
// TODO: otherwise channels may monopolize work.
void chaser_check::do_get_hashes(const handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& index = map_table_.at(0)->get<database::association::pos>();
    const auto size = index.size();
    const auto count = std::min(size, inventory_);
    const auto map = std::make_shared<database::associations>();

    /// Merge "moves" elements from one table to another.
    map->merge(index, index.begin(), std::next(index.begin(), count));
    ////LOGN("Hashes: ("
    ////    << size << " - "
    ////    << map->size() << " = "
    ////    << map_table_.at(0)->size() << ").");

    handler(error::success, map);
}

void chaser_check::do_put_hashes(const map_ptr& map,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    /// Merge "moves" elements from one table to another.
    map_table_.at(0)->merge(*map);

    const auto count = map_table_.at(0)->size();
    if (!is_zero(count))
        notify(error::success, chase::header, count);

    handler(error::success);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
