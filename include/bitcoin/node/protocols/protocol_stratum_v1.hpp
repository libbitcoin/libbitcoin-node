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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP

#include <memory>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/interfaces.hpp>
#include <bitcoin/node/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_stratum_v1
  : public node::protocol_rpc<interface::stratum_v1>,
    protected network::tracker<protocol_stratum_v1>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1> ptr;
    using rpc_interface = interface::stratum_v1;

    inline protocol_stratum_v1(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_rpc<rpc_interface>(session, channel, options),
        network::tracker<protocol_stratum_v1>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_mining_extranonce_subscribe(const code& ec,
        rpc_interface::mining_extranonce_subscribe) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
