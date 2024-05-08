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
#include <bitcoin/node/chasers/chaser_template.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_template

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_template::chaser_template(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

// TODO: initialize template state.
code chaser_template::start() NOEXCEPT
{
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_template::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    if (closed())
        return false;

    // TODO: also handle confirmed/unconfirmed.
    switch (event_)
    {
        case chase::transaction:
        {
            using namespace system;
            POST(do_transaction, possible_narrow_cast<transaction_t>(value));
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

// TODO: handle transaction graph change (may issue 'candidate').
void chaser_template::do_transaction(transaction_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
