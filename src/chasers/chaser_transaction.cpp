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

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

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

chaser_transaction::~chaser_transaction() NOEXCEPT
{
}

// start
// ----------------------------------------------------------------------------

// TODO: initialize tx graph from store, log and stop on error.
code chaser_transaction::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_transaction::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::confirmed)
    {
        POST(handle_confirmed, std::get<height_t>(value));
    }
}

// TODO: handle the new confirmed blocks (may issue 'transaction').
void chaser_transaction::handle_confirmed(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
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
