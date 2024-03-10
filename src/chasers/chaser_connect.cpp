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
#include <bitcoin/node/chasers/chaser_connect.hpp>

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_connect

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_connect::chaser_connect(full_node& node) NOEXCEPT
  : chaser(node)
{
}

chaser_connect::~chaser_connect() NOEXCEPT
{
}

// start
// ----------------------------------------------------------------------------

// TODO: initialize connect state.
code chaser_connect::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_connect::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::checked)
    {
        POST(handle_checked, std::get<height_t>(value));
    }
}

// TODO: handle the new checked blocks (may issue 'connected'/'unconnected').
void chaser_connect::handle_checked(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    // TODO: if on this block, advance position until encountering a block that
    // is not checked. It may not be possible to do this on height alone, since
    // an asynchronous reorganization may render the height ambiguous.
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
