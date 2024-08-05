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
      : Session(node, identifier), session(node)
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

    void attach_protocols(
        const network::channel::ptr& channel) NOEXCEPT override
    {
        constexpr auto bip130 = network::messages::level::bip130;
        constexpr auto headers = network::messages::level::headers_protocol;
        constexpr auto in = is_same_type<Session, network::session_inbound>;

        const auto headers_first = config().node.headers_first;
        const auto version = channel->negotiated_version();
        const auto self = session::shared_from_sibling<attach<Session>,
            network::session>();

        // Attach appropriate alert, reject, ping, and/or address protocols.
        Session::attach_protocols(channel);
        
        if (headers_first && version >= bip130)
        {
            channel->attach<protocol_header_in_70012>(self)->start();
            channel->attach<protocol_header_out_70012>(self)->start();
            if constexpr (!in)
            {
                channel->attach<protocol_block_in_31800>(self)->start();
            }
        }
        else if (headers_first && version >= headers)
        {
            channel->attach<protocol_header_in_31800>(self)->start();
            channel->attach<protocol_header_out_31800>(self)->start();
            if constexpr (!in)
            {
                channel->attach<protocol_block_in_31800>(self)->start();
            }
        }
        else
        {
            // Very hard to find < 31800 peer to connect with.
            // Blocks-first synchronization (no headers protocol).
            if constexpr (!in)
            {
                channel->attach<protocol_block_in>(self)->start();
            }
        }

        channel->attach<protocol_block_out>(self)->start();
        channel->attach<protocol_transaction_in>(self)->start();
        channel->attach<protocol_transaction_out>(self)->start();
        channel->attach<protocol_observer>(self)->start();
    }

    network::channel::ptr create_channel(const network::socket::ptr& socket,
        bool quiet) NOEXCEPT override
    {
        // This memory arena is NOT thread safe.
        return std::make_shared<network::channel>(session::get_memory(),
            network::session::log, socket, network::session::settings(),
            network::session::create_key(), quiet);
    }
};

} // namespace node
} // namespace libbitcoin

#endif
