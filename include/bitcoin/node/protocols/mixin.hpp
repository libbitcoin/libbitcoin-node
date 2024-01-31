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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_MIXIN_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_MIXIN_HPP

#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/sessions/session.hpp>
#include <bitcoin/node/protocols/protocol_block_in.hpp>
#include <bitcoin/node/protocols/protocol_block_out.hpp>
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>
#include <bitcoin/node/protocols/protocol_header_in_70012.hpp>
#include <bitcoin/node/protocols/protocol_header_out_31800.hpp>
#include <bitcoin/node/protocols/protocol_header_out_70012.hpp>
#include <bitcoin/node/protocols/protocol_transaction_in.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out.hpp>

namespace libbitcoin {
namespace node {

/// Session base class template for protocol attach mixin.
/// node::session does not derive from network::session (siblings).
/// This avoids the diamond inheritance problem between network/node.
/// For this reason protocol contructors are templatized on Session.
template <class Session>
class mixin
  : public Session, public node::session
{
public:
    mixin(full_node& node, uint64_t identifier) NOEXCEPT
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
        // Attach appropriate alert, reject, ping, and/or address protocols.
        Session::attach_protocols(channel);

        auto& self = *this;
        ////const auto version = channel->negotiated_version();
        ////
        ////if (version >= network::messages::level::bip130)
        ////{
        ////    channel->attach<protocol_header_in_70012>(self)->start();
        ////    channel->attach<protocol_header_out_70012>(self)->start();
        ////}
        ////else if (version >= network::messages::level::headers_protocol)
        ////{
        ////    channel->attach<protocol_header_in_31800>(self)->start();
        ////    channel->attach<protocol_header_out_31800>(self)->start();
        ////}

        constexpr auto performance = false;
        channel->attach<protocol_block_in>(self, performance)->start();
        ////channel->attach<protocol_block_out>(self)->start();
        ////channel->attach<protocol_transaction_in>(self)->start();
        ////channel->attach<protocol_transaction_out>(self)->start();
    }
};

typedef mixin<network::session_manual> session_manual;
typedef mixin<network::session_inbound> session_inbound;

class session_outbound
  : public mixin<network::session_outbound>
{
public:
    session_outbound(full_node& node, uint64_t identifier) NOEXCEPT
      : mixin(node, identifier)
    {
        // mixin(node, identifier)...
        // Set the current top for version protocol, before handshake.
        // Attach and execute appropriate version protocol.
    };

protected:
    void attach_protocols(
        const network::channel::ptr& channel) NOEXCEPT override
    {
        // Attach appropriate alert, reject, ping, and/or address protocols.
        network::session_outbound::attach_protocols(channel);

        auto& self = *this;
        constexpr auto performance = true;
        channel->attach<protocol_block_in>(self, performance)->start();
        ////channel->attach<protocol_block_out>(self)->start();
        ////channel->attach<protocol_transaction_in>(self)->start();
        ////channel->attach<protocol_transaction_out>(self)->start();
    }
};

} // namespace node
} // namespace libbitcoin

#endif
