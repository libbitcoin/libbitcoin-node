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
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API protocol_transaction_out
  : public network::protocol_events, track<protocol_transaction_out>
{
public:
    typedef std::shared_ptr<protocol_transaction_out> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_out(full_node& network, network::channel::ptr channel,
        blockchain::safe_chain& chain);

    /// Start the protocol.
    virtual void start();

private:
    void send_next_data(inventory_ptr inventory);
    void send_transaction(const code& ec, transaction_const_ptr message,
        size_t position, size_t height, inventory_ptr inventory);

    bool handle_receive_get_data(const code& ec,
        get_data_const_ptr message);
    bool handle_receive_fee_filter(const code& ec,
        fee_filter_const_ptr message);
    bool handle_receive_memory_pool(const code& ec,
        memory_pool_const_ptr message);

    void handle_fetch_mempool(const code& ec, inventory_ptr message);

    void handle_stop(const code& ec);
    void handle_send_next(const code& ec, inventory_ptr inventory);
    bool handle_transaction_pool(const code& ec,
        transaction_const_ptr message);

    // These are thread safe.
    blockchain::safe_chain& chain_;
    std::atomic<uint64_t> minimum_peer_fee_;
    ////std::atomic<bool> compact_to_peer_;
    const bool relay_to_peer_;
    const bool enable_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
