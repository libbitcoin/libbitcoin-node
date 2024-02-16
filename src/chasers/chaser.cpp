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
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser::chaser(full_node& node) NOEXCEPT
  : node_(node),
    strand_(node.service().get_executor()),
    subscriber_(node.event_subscriber()),
    reporter(node.log)
{
}

chaser::~chaser() NOEXCEPT
{
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

bool chaser::subscribe(event_handler&& handler) NOEXCEPT
{
    BC_ASSERT_MSG(node_.stranded(), "chaser");
    const auto ec = subscriber_.subscribe(std::move(handler));

    if (ec)
    {
        LOGF("Chaser subscribe fault, " << ec.message());
        return false;
    }

    return true;
}

// Posts to network strand (call from chaser strands).
void chaser::notify(const code& ec, chase event_, link value) NOEXCEPT
{
    boost::asio::post(node_.strand(),
        std::bind(&chaser::do_notify, this, ec, event_, value));
}

// Executed on network strand (handler should bounce to chaser strand).
void chaser::do_notify(const code& ec, chase event_, link value) NOEXCEPT
{
    BC_ASSERT_MSG(node_.stranded(), "chaser");
    subscriber_.notify(ec, event_, value);
}

void chaser::stop(const code&) NOEXCEPT
{
    ////LOGF("Chaser fault, " << ec.message());
    node_.close();
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
