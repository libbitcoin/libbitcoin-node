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
#include <bitcoin/node/chasers/chaser_header.hpp>

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_header::chaser_header(full_node& node) NOEXCEPT
  : chaser(node),
    tracker<chaser_header>(node.log)
{
    subscribe(std::bind(&chaser_header::handle_event, this, _1, _2));
}

void chaser_header::handle_event(const code& ec, chase value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_header::do_handle_event, this, ec, value));
}

void chaser_header::do_handle_event(const code& ec, chase value) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_header");

    // The code should be error::service_stopped when error::stop is set.
    if (ec)
        return;

    switch (value)
    {
        case chase::start:
            // TODO: initialize.
            break;
        default:
            return;
    }
}

void chaser_header::archive(const header::cptr& header) NOEXCEPT
{
    // Connect and push header on a stack (simulate tree).
    // When header time is current, reorg into candidate chain and then send
    // header event so that check can start downloading blocks.
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
