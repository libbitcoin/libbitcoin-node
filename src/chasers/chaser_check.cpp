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
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {
    
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Requires subscriber_ protection (call from node construct or node.strand).
chaser_check::chaser_check(full_node& node) NOEXCEPT
  : chaser(node)
{
}

chaser_check::~chaser_check() NOEXCEPT
{
}

// TODO: initialize check state.
code chaser_check::start() NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser_check");

    // get_all_unassociated_above(0)
    return subscribe(std::bind(&chaser_check::handle_event,
        this, _1, _2, _3));
}

void chaser_check::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_check::do_handle_event, this, ec, event_, value));
}

void chaser_check::do_handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_check");

    if (ec)
        return;

    switch (event_)
    {
        case chase::header:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            handle_header(std::get<height_t>(value));
            break;
        }
        default:
            return;
    }
}

// TODO: handle the new strong branch (may issue 'checked').
void chaser_check::handle_header(height_t) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_check");
    ////LOGN("Handle candidate organization above height (" << branch_point << ").");
    // get_all_unassociated_above(branch_point)
}

void chaser_check::checked(const block::cptr&) NOEXCEPT
{
    // Push checked block into store and issue 'checked' event so that connect
    // can connect the next blocks in order. Executes in caller thread.
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
