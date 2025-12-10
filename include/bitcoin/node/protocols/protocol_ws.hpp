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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WS_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WS_HPP

#include <memory>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {

// TODO: make this an intermediate base class for websocket.
// TODO: and then create a distinct concrete class for deployment.
class BCN_API protocol_ws
  : public node::protocol,
    public network::protocol_ws,
    protected network::tracker<protocol_ws>
{
public:
    typedef std::shared_ptr<protocol_ws> ptr;

    // Replace base class channel_t (network::channel_ws). 
    using channel_t = node::channel_ws;

    inline protocol_ws(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol_ws(session, channel, options),
        network::tracker<protocol_ws>(session->log)
    {
    }

    /// Public start is required.
    inline void start() NOEXCEPT override
    {
        BC_ASSERT(stranded());
        network::protocol_ws::start();
    }

    inline void stopping(const code& ec) NOEXCEPT override
    {
        BC_ASSERT(stranded());
        network::protocol_ws::stopping(ec);
    }

private:
    // This is thread safe.
    ////const options_t& options_;
};

} // namespace node
} // namespace libbitcoin

#endif
