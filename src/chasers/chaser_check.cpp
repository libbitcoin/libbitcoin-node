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

#include <functional>
#include <memory>
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
  : chaser(node)
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

    // Initialize map to all unassociated blocks starting at genesis.
    map_ = std::make_shared<database::context_map>(
        archive().get_all_unassociated_above(zero));

    return SUBSCRIBE_EVENT(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_check::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::header)
    {
        POST(handle_header, std::get<height_t>(value));
    }
}

// Stale branches are just be allowed to complete (still downloaded).
void chaser_check::handle_header(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(map_, "not started");

    const auto before = map_->size();
    map_->merge(archive().get_all_unassociated_above(branch_point));
    const auto after = map_->size();
    if (after > before)
        notify(error::success, chaser::chase::unassociated, {});
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

void chaser_check::do_get_hashes(const handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(map_, "not started");

    ///////////////////////////////////////////////////////////////////////////
    // TODO: get them!
    const auto hashes = std::make_shared<database::context_map>();
    ///////////////////////////////////////////////////////////////////////////

    handler(error::success, hashes);
}

void chaser_check::do_put_hashes(const map_ptr& map,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(map_, "not started");

    map_->merge(*map);
    handler(error::success);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
