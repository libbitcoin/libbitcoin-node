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
template <class Session>
class session_peer
  : public Session,
    public node::session
{
public:
    typedef std::shared_ptr<session_peer<Session>> ptr;
    using options_t = typename Session::options_t;
    using channel_t = node::channel_peer;

    /// Construct an instance.
    template <typename Node, typename... Args>
    session_peer(Node& node, Args&&... args) NOEXCEPT
      : Session(node, std::forward<Args>(args)...),
        node::session(node)
    {
    }

protected:
    using socket_ptr = network::socket::ptr;
    using channel_ptr = network::channel::ptr;

    // this-> is required for dependent base access in CRTP.

    channel_ptr create_channel(const socket_ptr& socket) NOEXCEPT override
    {
        BC_ASSERT(this->stranded());

        const auto channel = std::make_shared<channel_t>(
            this->get_memory(), this->log, socket, this->create_key(),
            this->config(), this->options());

        return std::static_pointer_cast<network::channel>(channel);
    }

    void attach_handshake(const channel_ptr& channel,
        network::result_handler&& handler) NOEXCEPT override
    {
        BC_ASSERT(channel->stranded());
        BC_ASSERT(channel->paused());

        // Set the current top for version protocol, before handshake.
        const auto top = this->archive().get_top_confirmed();
        const auto peer = std::dynamic_pointer_cast<channel_t>(channel);
        peer->set_start_height(top);

        // Attach and execute appropriate version protocol.
        Session::attach_handshake(channel, std::move(handler));
    }

    void attach_protocols(const channel_ptr& channel) NOEXCEPT override
    {
        BC_ASSERT(channel->stranded());
        BC_ASSERT(channel->paused());

        using namespace system;
        using namespace network;
        using namespace messages::peer;
        using base = session_peer<Session>;

        const auto self = this->template shared_from_base<base>();
        const auto relay = this->config().network.enable_relay;
        const auto delay = this->config().node.delay_inbound;
        const auto headers = this->config().node.headers_first;
        const auto node_network = to_bool(bit_and<uint64_t>
        (
            this->config().network.services_maximum,
            service::node_network
        ));
        const auto node_client_filters = to_bool(bit_and<uint64_t>
        (
            this->config().network.services_maximum,
            service::node_client_filters
        ));

        // Attach appropriate alert, reject, ping, and/or address protocols.
        Session::attach_protocols(channel);

        // Channel suspensions.
        channel->attach<protocol_observer>(self)->start();

        // Ready to relay blocks or block filters.
        const auto blocks_out = !delay || this->is_recent();

        ///////////////////////////////////////////////////////////////////////
        // bip152: "Upon receipt of a `sendcmpct` message with the first and
        // second integers set to 1, the node SHOULD announce new blocks by
        // sending a cmpctblock message." IOW at 70014 bip152 is optional.
        // This allows the node to support bip157 without supporting bip152.
        ///////////////////////////////////////////////////////////////////////

        const auto peer = std::dynamic_pointer_cast<channel_t>(channel);

        // Node must advertise node_client_filters or no out filters.
        if (node_client_filters && blocks_out &&
            peer->is_negotiated(level::bip157))
            channel->attach<protocol_filter_out_70015>(self)->start();

        // Node must advertise node_network or no in|out blocks|txs.
        if (!node_network)
            return;

        // Ready to relay transactions.
        const auto txs_in_out = relay && peer->is_negotiated(level::bip37) &&
            (!delay || is_current(true));

        // Peer advertises chain (blocks in).
        if (peer->is_peer_service(service::node_network))
        {
            if (headers && peer->is_negotiated(level::bip130))
            {
                channel->attach<protocol_header_in_70012>(self)->start();
                channel->attach<protocol_block_in_31800>(self)->start();

            }
            else if (headers && peer->is_negotiated(level::headers_protocol))
            {
                channel->attach<protocol_header_in_31800>(self)->start();
                channel->attach<protocol_block_in_31800>(self)->start();
            }
            else
            {
                // Very hard to find < 31800 peer to connect with.
                // Blocks-first synchronization (not base of block_in_31800).
                channel->attach<protocol_block_in_106>(self)->start();
            }
        }

        // Blocks are ready (blocks out).
        if (blocks_out)
        {
            if (headers && peer->is_negotiated(level::bip130))
            {
                channel->attach<protocol_header_out_70012>(self)->start();
                channel->attach<protocol_block_out_70012>(self)->start();
            }
            else if (headers && peer->is_negotiated(level::headers_protocol))
            {
                channel->attach<protocol_header_out_31800>(self)->start();
                channel->attach<protocol_block_out_106>(self)->start();
            }
            else
            {
                channel->attach<protocol_block_out_106>(self)->start();
            }
        }

        // Relay is configured, active, and txs are ready (txs in/out).
        if (txs_in_out)
        {
            if (peer->peer_version()->relay)
                channel->attach<protocol_transaction_out_106>(self)->start();
        }
    }
};

} // namespace node
} // namespace libbitcoin

#endif
