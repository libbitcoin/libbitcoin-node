/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

#include <unordered_set>
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
        tx_type_(session->node_settings().require_witness ?
            type_id::witness_tx : type_id::transaction),
        network::tracker<protocol_transaction_in_106>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Clear the request record, false if the tx was not requested.
    bool erase_requested(const system::hash_digest& hash) NOEXCEPT;

    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::peer::inventory::cptr& message) NOEXCEPT;

    /// Accept incoming transaction message.
    virtual bool handle_receive_transaction(const code& ec,
        const network::messages::peer::transaction::cptr& message) NOEXCEPT;
    virtual void handle_submit(const code& ec, size_t index,
        const gate_t::ptr& gate) NOEXCEPT;
    virtual void do_handle_submit(const code& ec,
        const gate_t::ptr& gate) NOEXCEPT;

private:
    /// Squash duplicates and provide constant time retrieval.
    using hashmap = std::unordered_set<system::hash_digest>;

    network::messages::peer::get_data create_get_data(
        const network::messages::peer::inventory& message) NOEXCEPT;

    // This is thread safe.
    const type_id tx_type_;

    // This is protected by strand.
    hashmap requested_{};
};

} // namespace node
} // namespace libbitcoin

#endif
