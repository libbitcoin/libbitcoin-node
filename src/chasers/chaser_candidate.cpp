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

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Requires subscriber_ protection (call from node construct or node.strand).
chaser_candidate::chaser_candidate(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// TODO: initialize candidate state.
code chaser_candidate::start() NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser_check");
    return subscribe(std::bind(&chaser_candidate::handle_event,
        this, _1, _2, _3));
}

void chaser_candidate::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_candidate::do_handle_event, this, ec, event_, value));
}

void chaser_candidate::do_handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_candidate");

    if (ec)
        return;

    switch (event_)
    {
        case chase::transaction:
        {
            BC_ASSERT(std::holds_alternative<transaction_t>(value));
            handle_transaction(std::get<transaction_t>(value));
            break;
        }
        default:
            return;
    }
}

// TODO: handle transaction graph change (may issue 'candidate').
void chaser_candidate::handle_transaction(transaction_t tx) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_candidate");
    LOGN("Handle transaction pool updated (" << tx << ").");
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
