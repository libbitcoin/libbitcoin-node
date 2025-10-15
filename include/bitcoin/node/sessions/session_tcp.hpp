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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_TCP_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_TCP_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class session_tcp
  : public network::session_tcp, public node::session
{
public:
    typedef std::shared_ptr<session_tcp> ptr;
    using options_t = network::session_tcp::options_t;

    session_tcp(full_node& node, uint64_t identifier,
        const options_t& options) NOEXCEPT;

protected:
    inline bool enabled() const NOEXCEPT override;
};

// TODO: move all sessions up from network.
////using session_web        = network::session_server<protocol_web,        session_tcp>;
using session_explore        = network::session_server<protocol_explore,    session_tcp>;
////using session_websocket  = network::session_server<protocol_websocket,  session_tcp>;
////using session_bitcoind   = network::session_server<protocol_bitcoind,   session_tcp>;
////using session_electrum   = network::session_server<protocol_electrum,   session_tcp>;
////using session_stratum_v1 = network::session_server<protocol_stratum_v1, session_tcp>;
////using session_stratum_v2 = network::session_server<protocol_stratum_v2, session_tcp>;

} // namespace node
} // namespace libbitcoin

#endif
