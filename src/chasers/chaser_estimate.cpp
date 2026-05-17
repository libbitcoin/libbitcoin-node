/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser_estimate.hpp>

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_estimate

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_estimate::chaser_estimate(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

code chaser_estimate::start() NOEXCEPT
{
    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_estimate::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Keep updating the fee accumulator (blocks continue organizing).
    ////if (suspended())
    ////    return true;

    switch (event_)
    {
        case chase::organized:
        {
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_organized, std::get<header_t>(value));
            break;
        }
        case chase::reorganized:
        {
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_reorganized, std::get<header_t>(value));
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

void chaser_estimate::do_organized(header_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

void chaser_estimate::do_reorganized(header_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
