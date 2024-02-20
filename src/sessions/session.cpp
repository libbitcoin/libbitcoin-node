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
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

session::session(full_node& node) NOEXCEPT
  : node_(node)
{
}

session::~session() NOEXCEPT
{
}

void session::performance(uint64_t, uint64_t,
    network::result_handler&& handler) NOEXCEPT
{
    // TODO: do work on network strand and then invoke handler.
    boost::asio::post(node_.strand(), std::bind(handler, error::unknown));
}

void session::organize(const system::chain::header::cptr& header) NOEXCEPT
{
    node_.organize(header);
}

void session::organize(const system::chain::block::cptr& block) NOEXCEPT
{
    node_.organize(block);
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
