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
#include <bitcoin/node/sessions/session.hpp>

#include <functional>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

using namespace system::chain;
using namespace network;

session::session(full_node& node) NOEXCEPT
  : node_(node)
{
}

session::~session() NOEXCEPT
{
}

void session::performance(uint64_t, uint64_t, result_handler&& handler) NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    boost::asio::post(node_.strand(), std::bind(handler, error::unknown));
    BC_POP_WARNING()
}

void session::organize(const header::cptr& header,
    result_handler&& handler) NOEXCEPT
{
    node_.organize(header, std::move(handler));
}

void session::organize(const block::cptr& block,
    result_handler&& handler) NOEXCEPT
{
    node_.organize(block, std::move(handler));
}

void session::get_hashes(chaser_check::handler&& handler) NOEXCEPT
{
    node_.get_hashes(std::move(handler));
}

void session::put_hashes(const chaser_check::map& map,
    network::result_handler&& handler) NOEXCEPT
{
    node_.put_hashes(map, std::move(handler));
}

const configuration& session::config() const NOEXCEPT
{
    return node_.config();
}

full_node::query& session::archive() const NOEXCEPT
{
    return node_.archive();
}
} // namespace node
} // namespace libbitcoin
