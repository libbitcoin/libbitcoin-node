/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_PEER_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_PEER_HPP

#include <memory>
#include <utility>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocols.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Template for network::session derivation with node::session.
/// node::session does not derive from network::session (siblings).
/// This avoids the diamond inheritance problem between network/node.
template <class NetworkSession>
class session_peer
  : public node::session,
    public NetworkSession
{
public:
    typedef std::shared_ptr<session_peer<NetworkSession>> ptr;
    using options_t = typename NetworkSession::options_t;
    using channel_t = node::channel_peer;

    /// Construct an instance.
    template <typename Node, typename... Args>
    session_peer(Node& node, Args&&... args) NOEXCEPT
      : node::session(node),
        NetworkSession(node, std::forward<Args>(args)...)
    {
    }

protected:
    using socket_ptr = network::socket::ptr;
    using channel_ptr = network::channel::ptr;

    inline channel_ptr create_channel(
        const socket_ptr& socket) NOEXCEPT override;
    inline void attach_handshake(const channel_ptr& channel,
        network::result_handler&& handler) NOEXCEPT override;
    inline void attach_protocols(const channel_ptr& channel) NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#define TEMPLATE template <typename NetworkSession>
#define CLASS session_peer<NetworkSession>

////BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

#include <bitcoin/node/impl/sessions/session_peer.ipp>

////BC_POP_WARNING()

#undef CLASS
#undef TEMPLATE

#endif
