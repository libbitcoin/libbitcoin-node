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
        headers_(node.config().node.headers_first),
        blockchain_(to_bool(system::bit_and<uint64_t>
        (
            node.config().network.services_maximum,
            network::messages::service::node_network
        )))
    {
    };

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
        constexpr auto perform = is_same_type<Session, session_outbound>;
        const auto self = session::shared_from_sibling<attach<Session>,
            network::session>();

        // Attach appropriate alert, reject, ping, and/or address protocols.
        Session::attach_protocols(channel);

        // Channel suspensions.
        channel->attach<protocol_observer>(self)->start();

        // We must advertise node_network or there is no in|out of blocks|txs. 
        if (!blockchain_)
            return;

        // Peer advertises chain (blocks in).
        if (peer_blocks(channel))
        {
            if (send_headers_version(channel))
            {
                channel->attach<protocol_header_in_70012>(self)->start();
                channel->attach<protocol_block_in_31800>(self, perform)->start();

            }
            else if (headers_version(channel))
            {
                channel->attach<protocol_header_in_31800>(self)->start();
                channel->attach<protocol_block_in_31800>(self, perform)->start();
            }
            else
            {
                // Very hard to find < 31800 peer to connect with.
                // Blocks-first synchronization (no headers protocol).
                channel->attach<protocol_block_in_106>(self)->start();
            }
        }

        // Blocks are ready (blocks out).
        if (blocks_out())
        {
            if (send_headers_version(channel))
            {
                channel->attach<protocol_header_out_70012>(self)->start();
                channel->attach<protocol_block_out_70012>(self)->start();
            }
            else if (headers_version(channel))
            {
                channel->attach<protocol_header_out_31800>(self)->start();
                channel->attach<protocol_block_out_106>(self)->start();
            }
            else
            {
                // Very hard to find < 31800 peer to connect with.
                // Blocks-first synchronization (no headers protocol).
                channel->attach<protocol_block_out_106>(self)->start();
            }
        }

        // Txs are ready (txs in/out).
        if (tx_relay())
        {
            if (relay_version(channel))
            {
                channel->attach<protocol_transaction_in_70001>(self)->start();
                channel->attach<protocol_transaction_out_70001>(self)->start();
            }
            else
            {
                channel->attach<protocol_transaction_in_106>(self)->start();
                channel->attach<protocol_transaction_out_106>(self)->start();
            }
        }
    }

    network::channel::ptr create_channel(const network::socket::ptr& socket,
        bool quiet) NOEXCEPT override
    {
        return std::make_shared<network::channel>(node::session::get_memory(),
            network::session::log, socket, network::session::settings(),
            network::session::create_key(), quiet);
    }

private:
    inline bool tx_relay() const NOEXCEPT
    {
        // delay_inbound also defers accepting inbound connections.
        return !config().node.delay_inbound || is_current(true);
    }

    inline bool blocks_out() const NOEXCEPT
    {
        // delay_inbound also defers accepting inbound connections.
        return !config().node.delay_inbound || is_recent();
    }

    inline bool relay_version(
        const network::channel::ptr& channel) const NOEXCEPT
    {
        constexpr auto bip37 = network::messages::level::bip37;
        return channel->negotiated_version() >= bip37;
    }

    inline bool send_headers_version(
        const network::channel::ptr& channel) const NOEXCEPT
    {
        constexpr auto bip130 = network::messages::level::bip130;
        return headers_ && channel->negotiated_version() >= bip130;
    }

    inline bool headers_version(
        const network::channel::ptr& channel) const NOEXCEPT
    {
        constexpr auto headers = network::messages::level::headers_protocol;
        return headers_ && channel->negotiated_version() >= headers;
    }

    inline bool peer_blocks(
        const network::channel::ptr& channel) const NOEXCEPT
    {
        using namespace system;
        return to_bool(bit_and<uint64_t>
        (
            channel->peer_version()->services,
            network::messages::service::node_network
        ));
    }

    bool headers_;
    bool blockchain_;
};

} // namespace node
} // namespace libbitcoin

#endif
