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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WEBSOCKET_SHAKE_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WEBSOCKET_SHAKE_HPP

#include <memory>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {

// TODO: make this an intermediate base class for websocket_shake.
// TODO: and then create a distinct concrete class for deployment.
class BCN_API protocol_websocket_shake
  : public network::protocol_websocket_shake,
    public node::protocol,
    protected network::tracker<protocol_websocket_shake>
{
public:
    typedef std::shared_ptr<protocol_websocket_shake> ptr;

    // Replace base class channel_t (network::channel_http). 
    using channel_t = node::channel_http;

    protocol_websocket_shake(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : network::protocol_websocket_shake(session, channel, options),
        node::protocol(session, channel),
        network::tracker<protocol_websocket_shake>(session->log)
    {
    }

    /// Public start is required.
    void start() NOEXCEPT override
    {
        network::protocol_websocket_shake::start();
    }

private:
    // This is thread safe.
    ////const options_t& options_;
};

} // namespace node
} // namespace libbitcoin

#endif
