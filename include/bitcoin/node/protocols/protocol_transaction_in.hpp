/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP

#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_in
  : public network::protocol_events, track<protocol_transaction_in>
{
public:
    typedef std::shared_ptr<protocol_transaction_in> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_in(network::p2p& network,
        network::channel::ptr channel, blockchain::block_chain& blockchain,
        blockchain::transaction_pool& pool);

    /// Start the protocol.
    virtual void start();

private:
    typedef chain::point::indexes index_list;
    typedef message::inventory::ptr inventory_ptr;
    typedef message::transaction_message::ptr transaction_ptr;

    void send_get_data(const code& ec, const hash_list& hashes);
    bool handle_receive_inventory(const code& ec, inventory_ptr message);
    bool handle_receive_transaction(const code& ec, transaction_ptr message);
    void handle_store_validated(const code& ec, 
        const message::transaction_message& transaction,
        const hash_digest& hash, const index_list& unconfirmed);
    void handle_store_confirmed(const code& ec,
        const message::transaction_message& transaction,
        const hash_digest& hash);

    void handle_stop(const code&);
    bool peer_supports_memory_pool_message();

    blockchain::block_chain& blockchain_;
    blockchain::transaction_pool& pool_;
    const bool relay_from_peer_;
};

} // namespace node
} // namespace libbitcoin

#endif
