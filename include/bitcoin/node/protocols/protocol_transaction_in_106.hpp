/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_106_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_106_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_transaction_in_106
  : public node::protocol_peer,
    protected network::tracker<protocol_transaction_in_106>
{
public:
    typedef std::shared_ptr<protocol_transaction_in_106> ptr;

    protocol_transaction_in_106(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_peer(session, channel),
        ////tx_type_(session->network_settings().witness_node() ?
        ////    type_id::witness_tx : type_id::transaction),
        network::tracker<protocol_transaction_in_106>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::peer::inventory::cptr& message) NOEXCEPT;

private:
    // This is thread safe.
    ////const type_id tx_type_;
};

} // namespace node
} // namespace libbitcoin

#endif
