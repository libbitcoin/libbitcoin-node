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
#include <bitcoin/node/chasers/chaser.hpp>

#include <functional>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser

using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Requires subscriber_ protection (call from node construct or node.strand).
chaser::chaser(full_node& node) NOEXCEPT
  : node_(node),
    strand_(node.service().get_executor()),
    subscriber_(node.event_subscriber())
    ////reporter(node.log)
{
}

chaser::~chaser() NOEXCEPT
{
}

bool chaser::closed() const NOEXCEPT
{
    return node_.closed();
}

const node::configuration& chaser::config() const NOEXCEPT
{
    return node_.config();
}

chaser::query& chaser::archive() const NOEXCEPT
{
    return node_.archive();
}

asio::strand& chaser::strand() NOEXCEPT
{
    return strand_;
}

bool chaser::stranded() const NOEXCEPT
{
    return strand_.running_in_this_thread();
}

bool chaser::node_stranded() const NOEXCEPT
{
    return node_.stranded();
}

code chaser::subscribe(event_handler&& handler) NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser");
    return subscriber_.subscribe(std::move(handler));
}

// Posts to network strand (call from chaser strands).
void chaser::notify(const code& ec, chase event_, link value) NOEXCEPT
{
    POST_3(do_notify, ec, event_, value);
}

// Executed on network strand (handler should bounce to chaser strand).
void chaser::do_notify(const code& ec, chase event_, link value) NOEXCEPT
{
    BC_ASSERT_MSG(node_stranded(), "chaser");
    subscriber_.notify(ec, event_, value);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
