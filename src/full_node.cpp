/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

full_node::full_node(query_t& query, const node::configuration& configuration,
    const network::logger& log) NOEXCEPT
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

query_t& full_node::query() NOEXCEPT
{
    return query_;
}

const configuration& full_node::config() const NOEXCEPT
{
    return config_;
}
// Return is a pointer cast.
// Session attachment passes p2p and variable args.
// To avoid templatizing on p2p, pass node as first arg.

network::session_manual::ptr full_node::attach_manual_session() NOEXCEPT
{
    return p2p::attach<node::session<network::session_manual>>(*this, *this);
}

network::session_inbound::ptr full_node::attach_inbound_session() NOEXCEPT
{
    return p2p::attach<node::session<network::session_inbound>>(*this, *this);
}

network::session_outbound::ptr full_node::attach_outbound_session() NOEXCEPT
{
    return p2p::attach<node::session<network::session_outbound>>(*this, *this);
}

} // namespace node
} // namespace libbitcoin
