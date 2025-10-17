/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_TCP_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_TCP_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API session_tcp
  : public network::session_tcp,
    public node::session
{
public:
    typedef std::shared_ptr<session_tcp> ptr;
    using options_t = network::session_tcp::options_t;

    // (network::net&) cast due to full_node forward ref (inheritance hidden).
    session_tcp(full_node& node, uint64_t identifier,
        const options_t& options) NOEXCEPT
      : network::session_tcp((network::net&)node, identifier, options),
        node::session(node)
    {
    }

protected:
    bool enabled() const NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#endif
