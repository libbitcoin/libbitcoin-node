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

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_connect::chaser_connect(full_node& node) NOEXCEPT
  : chaser(node),
    tracker<chaser_connect>(node.log)
{
    subscribe(std::bind(&chaser_connect::handle_event, this, _1, _2, _3));
}

void chaser_connect::handle_event(const code& ec, chase event_,
    link value) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_connect::do_handle_event, this, ec, event_, value));
}

void chaser_connect::do_handle_event(const code& ec, chase event_,
    link) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_connect");

    if (ec)
        return;

    switch (event_)
    {
        case chase::start:
        {
            handle_start();
            break;
        }
        case chase::checked:
        {
            handle_checked();
            break;
        }
        default:
            return;
    }
}

// TODO: initialize connect state.
void chaser_connect::handle_start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_connect");
}

// TODO: handle the new checked blocks (may issue 'connected').
void chaser_connect::handle_checked() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "chaser_connect");
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
