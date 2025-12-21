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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_RPC_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_RPC_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

 // Only session.hpp.
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {
    
/// Abstract base for RPC protocols, thread safe.
class BCN_API protocol_rpc
  : public node::protocol,
    public network::protocol_rpc
{
public:
    using channel_t = node::channel_rpc;

protected:
    inline protocol_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol_rpc(session, channel, options)
    {
    }
};

} // namespace node
} // namespace libbitcoin

#endif
