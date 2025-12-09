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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HTTP_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HTTP_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

 // Only session.hpp.
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {
    
/// Abstract base for HTTP protocols, thread safe.
class BCN_API protocol_http
  : public node::protocol,
    public network::protocol_http
{
public:
    // Replace base class channel_t (network::channel_http). 
    using channel_t = node::channel_http;

protected:
    protocol_http(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol_http(session, channel, options),
        channel_(std::static_pointer_cast<node::channel_tcp>(channel)),
        session_(session)
    {
    }

private:
    // This derived channel requires stranded calls, base is thread safe.
    const node::channel_tcp::ptr channel_;

    // This is thread safe.
    const node::session::ptr session_;
};

} // namespace node
} // namespace libbitcoin

#endif
