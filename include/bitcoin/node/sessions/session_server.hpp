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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_SERVER_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_SERVER_HPP

#include <memory>
#include <utility>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

class full_node;

/// Declare a concrete instance of this type for client-server protocols built
/// on tcp/ip. session_base processing performs all connection management and
/// session tracking. This includes start/stop/disable/enable/black/whitelist.
/// Protocol must declare options_t and channel_t. This protocol is constructed
/// and attached to a constructed instance of channel_t. The protocol construct
/// and attachment can be overridden and/or augmented with other protocols.
template <typename Protocol>
class session_server
  : public network::session_server,
    public node::session,
    protected network::tracker<session_server<Protocol>>
{
public:
    typedef std::shared_ptr<session_server<Protocol>> ptr;

    /// The protocol must define these public types.
    using options_t = typename Protocol::options_t;
    using channel_t = typename Protocol::channel_t;

    /// Construct an instance (network should be started).
    inline session_server(full_node& node, uint64_t identifier,
        const options_t& options) NOEXCEPT
      : network::session_server((network::net&)node, identifier, options),
        node::session(node),
        network::tracker<session_server<Protocol>>(node),
        options_(options)
    {
    }

protected:
    using socket_ptr = network::socket::ptr;
    using channel_ptr = network::channel::ptr;

    /// Inbound connection attempts are dropped unless confirmed is current.
    /// Used instead of suspension because that has independent start/stop.
    inline bool enabled() const NOEXCEPT override
    {
        return !config().node.delay_inbound || is_recent();
    }

    /// Override to construct channel. This allows the implementation to pass
    /// other values to protocol construction and/or select the desired channel
    /// based on available factors (e.g. a distinct protocol version).
    inline channel_ptr create_channel(
        const socket_ptr& socket) NOEXCEPT override
    {
        BC_ASSERT(stranded());

        const auto channel = std::make_shared<channel_t>(log, socket,
            create_key(), config(), options_);

        return std::static_pointer_cast<network::channel>(channel);
    }

    /// Override to implement a connection handshake as required. By default
    /// this is bypassed, which applies to basic http services. A handshake
    /// is used to implement TLS and WebSocket upgrade from http (for example).
    /// Handshake protocol(s) must invoke handler one time at completion.
    /// Use std::dynamic_pointer_cast<channel_t>(channel) to obtain channel_t.
    inline void attach_handshake(const channel_ptr& channel,
        network::result_handler&& handler) NOEXCEPT override
    {
        BC_ASSERT(channel->stranded());
        BC_ASSERT(channel->paused());

        network::session_server::attach_handshake(channel, std::move(handler));
    }

    /// Overridden to set channel protocols. This allows the implementation to
    /// pass other values to protocol construction and/or select the desired
    /// protocol based on available factors (e.g. a distinct protocol version).
    /// Use std::dynamic_pointer_cast<channel_t>(channel) to obtain channel_t.
    inline void attach_protocols(const channel_ptr& channel) NOEXCEPT override
    {
        BC_ASSERT(channel->stranded());
        BC_ASSERT(channel->paused());

        const auto self = shared_from_base<session_server<Protocol>>();
        channel->attach<Protocol>(self, options_)->start();
    }

private:
    // This is thread safe.
    const options_t& options_;
};

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin

#endif
