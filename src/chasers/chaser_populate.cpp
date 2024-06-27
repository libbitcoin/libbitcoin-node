/**
 * Copyright (c) 2011-2024 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser_populate.hpp>

#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_populate

////using namespace system;
////using namespace system::chain;
////using namespace database;
////using namespace network;
using namespace std::placeholders;

////// Shared pointers required for lifetime in handler parameters.
////BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
////BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
////BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_populate::chaser_populate(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start/stop
// ----------------------------------------------------------------------------

code chaser_populate::start() NOEXCEPT
{
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_populate::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::checked:
        {
            POST(do_checked, height_t{});
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

void chaser_populate::do_checked(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

////BC_POP_WARNING()
////BC_POP_WARNING()
////BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
