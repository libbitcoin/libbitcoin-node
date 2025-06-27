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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_70001_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_70001_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_in_106.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_transaction_in_70001
  : public protocol_transaction_in_106,
    protected network::tracker<protocol_transaction_in_70001>
{
public:
    typedef std::shared_ptr<protocol_transaction_in_70001> ptr;

    template <typename SessionPtr>
    protocol_transaction_in_70001(const SessionPtr& session,
        const channel_ptr& channel) NOEXCEPT
      : protocol_transaction_in_106(session, channel),
        relay_(session->config().network.enable_relay),
        network::tracker<protocol_transaction_in_70001>(session->log)
    {
    }

protected:
    /// Accept incoming inventory message.
    bool handle_receive_inventory(const code& ec,
        const network::messages::inventory::cptr& message) NOEXCEPT override;

private:
    bool relay_;
};

} // namespace node
} // namespace libbitcoin

#endif
