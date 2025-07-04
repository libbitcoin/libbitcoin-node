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
#ifndef LIBBITCOIN_NODE_SESSIONS_ATTACH_HPP
#define LIBBITCOIN_NODE_SESSIONS_ATTACH_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocols.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

class session_outbound;

/// Session base class template for protocol attachment.
/// node::session does not derive from network::session (siblings).
/// This avoids the diamond inheritance problem between network/node.
/// Protocol contructors are templatized on Session, obtaining session.
template <class Session>
class attach
  : public Session, public node::session
{
public:
    attach(full_node& node, uint64_t identifier) NOEXCEPT
      : Session(node, identifier),
        session(node),
        relay_(node.config().network.enable_relay),
        delay_(node.config().node.delay_inbound),
        headers_(node.config().node.headers_first),
        node_network_(to_bool(system::bit_and<uint64_t>
        (
            node.config().network.services_maximum,
            network::messages::service::node_network
        ))),
        node_client_filters_(to_bool(system::bit_and<uint64_t>
        (
            node.config().network.services_maximum,
            network::messages::service::node_client_filters
        )))
    {
    }

protected:
    void attach_handshake(const network::channel::ptr& channel,
        network::result_handler&& handler) NOEXCEPT override
    {
        // Set the current top for version protocol, before handshake.
        channel->set_start_height(archive().get_top_confirmed());

        // Attach and execute appropriate version protocol.
        Session::attach_handshake(channel, std::move(handler));
    }

    void attach_protocols(const network::channel::ptr& channel) NOEXCEPT override
    {
        using namespace network::messages;
        const auto self = session::shared_from_sibling<attach<Session>,
            network::session>();

        // Attach appropriate alert, reject, ping, and/or address protocols.
        Session::attach_protocols(channel);

        // Channel suspensions.
        channel->attach<protocol_observer>(self)->start();

        // Ready to relay blocks or block filters.
        const auto blocks_out = !delay_ || is_recent();

        ///////////////////////////////////////////////////////////////////////
        // bip152: "Upon receipt of a `sendcmpct` message with the first and
        // second integers set to 1, the node SHOULD announce new blocks by
        // sending a cmpctblock message." IOW at 70014 bip152 is optional.
        // This allows the node to support bip157 without supporting bip152.
        ///////////////////////////////////////////////////////////////////////

        // Node must advertise node_client_filters or no out filters.
        if (node_client_filters_ && blocks_out &&
            channel->is_negotiated(level::bip157))
            channel->attach<protocol_filter_out_70015>(self)->start();

        // Node must advertise node_network or no in|out blocks|txs.
        if (!node_network_)
            return;

        // Ready to relay transactions.
        const auto txs_in_out = relay_ &&
            channel->is_negotiated(level::bip37) &&
            (!delay_ || is_current(true));

        // Peer advertises chain (blocks in).
        if (channel->is_peer_service(service::node_network))
        {
            if (headers_ && channel->is_negotiated(level::bip130))
            {
                channel->attach<protocol_header_in_70012>(self)->start();
                channel->attach<protocol_block_in_31800>(self)->start();

            }
            else if (headers_ && channel->is_negotiated(level::headers_protocol))
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
            if (headers_ && channel->is_negotiated(level::bip130))
            {
                channel->attach<protocol_header_out_70012>(self)->start();
                channel->attach<protocol_block_out_70012>(self)->start();
            }
            else if (headers_ && channel->is_negotiated(level::headers_protocol))
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
            channel->attach<protocol_transaction_in_106>(self)->start();
            if (channel->peer_version()->relay)
                channel->attach<protocol_transaction_out_106>(self)->start();
        }
    }

    network::channel::ptr create_channel(const network::socket::ptr& socket,
        bool quiet) NOEXCEPT override
    {
        return std::static_pointer_cast<network::channel>(
            std::make_shared<node::channel>(node::session::get_memory(),
                network::session::log, socket, node::session::config(),
                network::session::create_key(), quiet));
    }

private:
    // These are thread safe.
    const bool relay_;
    const bool delay_;
    const bool headers_;
    const bool node_network_;
    const bool node_client_filters_;
};

} // namespace node
} // namespace libbitcoin

#endif
