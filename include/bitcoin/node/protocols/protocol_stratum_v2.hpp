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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_STRATUM_V2_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_STRATUM_V2_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_stratum_v2
  : public node::protocol,
    public network::protocol,
    protected network::tracker<protocol_stratum_v2>
{
public:
    typedef std::shared_ptr<protocol_stratum_v2> ptr;
    using channel_t = node::channel_stratum_v2;

    inline protocol_stratum_v2(const auto& session,
        const network::channel::ptr& channel, const options_t&) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol(session, channel),
        network::tracker<protocol_stratum_v2>(session->log)
    {
    }

    // TODO: subscribe to and handle messages.
    inline void start() NOEXCEPT override
    {
        network::protocol::start();
    }
};

} // namespace node
} // namespace libbitcoin

#endif
