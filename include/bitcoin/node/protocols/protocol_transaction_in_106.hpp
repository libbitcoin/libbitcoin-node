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

#include <deque>
#include <unordered_map>
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
        network::tracker<protocol_transaction_in_106>(session->log),
        tx_type_(session->node_settings().require_witness ?
            type_id::witness_tx : type_id::transaction)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Record the requested tx as not found, false if it was not requested.
    bool set_not_found(const system::hash_digest& hash) NOEXCEPT;

    /// The inventory type of tx announcements accepted from the peer.
    virtual type_id inventory_type() const NOEXCEPT;

    /// The inventory type of tx requests sent to the peer.
    virtual type_id get_data_type() const NOEXCEPT;

    /// The hash by which the peer identifies the tx.
    virtual system::hash_digest tx_identifier(
        const system::chain::transaction& tx) const NOEXCEPT;

    /// The tx identified by the hash is pooled or confirmed.
    virtual bool is_known(const system::hash_digest& hash) const NOEXCEPT;

    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::peer::inventory::cptr& message) NOEXCEPT;

    /// Accept incoming transaction message.
    virtual bool handle_receive_transaction(const code& ec,
        const network::messages::peer::transaction::cptr& message) NOEXCEPT;
    /// Whole if every tx requested of the announcement was in the set.
    virtual void handle_submit(const code& ec, size_t index, size_t count,
        bool whole, const gate_t::ptr& gate) NOEXCEPT;
    virtual void do_handle_submit(const code& ec, size_t count, bool whole,
        const gate_t::ptr& gate) NOEXCEPT;

private:
    /// The txs requested of one announcement, in its order, as they arrive.
    struct announcement
    {
        system::hashes hashes{};
        system::chain::transaction_cptrs txs{};
        size_t open{};
        size_t weight{};
        bool whole{ true };
    };

    /// The set and slot of a requested tx (the set is late once submitted).
    struct request
    {
        size_t set{};
        size_t slot{};
    };

    using requests = std::unordered_map<system::hash_digest, request>;

    network::messages::peer::get_data create_get_data(
        const network::messages::peer::inventory& message) NOEXCEPT;

    /// Submit the sets before the set, as the peer answers in order.
    void advance(size_t set) NOEXCEPT;

    /// Submit the first set as it stands.
    void flush() NOEXCEPT;

    // This is thread safe.
    const type_id tx_type_;

    // These are protected by strand.
    requests requested_{};
    std::deque<announcement> sets_{};
    size_t first_{};
};

} // namespace node
} // namespace libbitcoin

#endif
