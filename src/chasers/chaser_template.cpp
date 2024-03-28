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
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_template::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    // TODO: also handle confirmed/unconfirmed.
    switch (event_)
    {
        case chase::transaction:
        {
            BC_ASSERT(std::holds_alternative<transaction_t>(value));
            POST(do_transaction, std::get<transaction_t>(value));
            break;
        }
        case chase::header:
        case chase::download:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::pause:
        case chase::resume:
        case chase::bump:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmed:
        case chase::unpreconfirmed:
        case chase::confirmed:
        case chase::unconfirmed:
        case chase::disorganized:
        ////case chase::transaction:
        case chase::template_:
        case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

// TODO: handle transaction graph change (may issue 'candidate').
void chaser_template::do_transaction(transaction_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
