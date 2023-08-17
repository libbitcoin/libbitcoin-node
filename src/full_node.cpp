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
#include <bitcoin/node/full_node.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

using namespace network;

full_node::full_node(query& query, const configuration& configuration,
    const logger& log) NOEXCEPT
  : p2p(configuration.network, log),
    config_(configuration),
    query_(query)
{
}

void full_node::start(result_handler&& handler) NOEXCEPT
{
    if (!query_.is_initialized())
    {
        handler(error::store_uninitialized);
        return;
    }

    p2p::start(std::move(handler));
}

void full_node::run(result_handler&& handler) NOEXCEPT
{
    p2p::run(std::move(handler));
}

full_node::query& full_node::archive() const NOEXCEPT
{
    return query_;
}

const configuration& full_node::config() const NOEXCEPT
{
    return config_;
}

session_manual::ptr full_node::attach_manual_session() NOEXCEPT
{
    return p2p::attach<mixin<session_manual>>(*this);
}

session_inbound::ptr full_node::attach_inbound_session() NOEXCEPT
{
    return p2p::attach<mixin<session_inbound>>(*this);
}

session_outbound::ptr full_node::attach_outbound_session() NOEXCEPT
{
    return p2p::attach<mixin<session_outbound>>(*this);
}

} // namespace node
} // namespace libbitcoin
