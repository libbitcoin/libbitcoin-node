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
#include <bitcoin/node/chasers/chaser_candidate.hpp>

#include <functional>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_candidate

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_candidate::chaser_candidate(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

// TODO: initialize candidate state.
code chaser_candidate::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_candidate::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    // TODO: also handle confirmed/unconfirmed.
    if (event_ == chase::transaction)
    {
        BC_ASSERT(std::holds_alternative<transaction_t>(value));
        POST(handle_transaction, std::get<transaction_t>(value));
    }
}

// TODO: handle transaction graph change (may issue 'candidate').
void chaser_candidate::handle_transaction(transaction_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
