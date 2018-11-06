/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API protocol_transaction_in
  : public network::protocol_events, track<protocol_transaction_in>
{
public:
    typedef std::shared_ptr<protocol_transaction_in> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_in(full_node& network, network::channel::ptr channel,
        blockchain::safe_chain& chain);

    /// Start the protocol.
    virtual void start();

private:
    void send_get_transactions(transaction_const_ptr message);
    void send_get_data(const code& ec, get_data_ptr message);

    bool handle_receive_inventory(const code& ec, inventory_const_ptr message);
    bool handle_receive_transaction(const code& ec,
        transaction_const_ptr message);
    void handle_store_transaction(const code& ec,
        transaction_const_ptr message);

    void handle_stop(const code&);

    // These are thread safe.
    blockchain::safe_chain& chain_;
    const uint64_t minimum_relay_fee_;
    const bool relay_from_peer_;
    const bool refresh_pool_;
    const bool require_witness_;
    const bool peer_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
