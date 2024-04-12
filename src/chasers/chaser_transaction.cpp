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
#include <bitcoin/node/chasers/chaser_transaction.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_transaction
    
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_transaction::chaser_transaction(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

// TODO: initialize tx graph from store, log and stop on error.
code chaser_transaction::start() NOEXCEPT
{
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_transaction::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    using namespace system;
    switch (event_)
    {
        case chase::confirmable:
        {
            POST(do_confirmed, possible_narrow_cast<header_t>(value));
            break;
        }
        case chase::stop:
        {
            // TODO: handle fault.
            break;
        }
        case chase::start:
        case chase::pause:
        case chase::resume:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::block:
        case chase::header:
        case chase::download:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmable:
        case chase::unpreconfirmable:
        ////case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        ////case chase::stop:
        {
            break;
        }
    }
}

// TODO: handle the new confirmed blocks (may issue 'transaction').
void chaser_transaction::do_confirmed(header_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    notify(error::success, chase::transaction, transaction_t{});
}

// methods
// ----------------------------------------------------------------------------

void chaser_transaction::store(const transaction::cptr&) NOEXCEPT
{
    // Push new checked tx into store and update DAG. Issue transaction event
    // so that candidate may construct a new template.
}

// private
void chaser_transaction::do_store(const transaction::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    // TODO: validate and store transaction.
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
