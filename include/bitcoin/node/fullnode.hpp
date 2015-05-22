/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_FULLNODE_HPP
#define LIBBITCOIN_NODE_FULLNODE_HPP

#include <string>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/session.hpp>
#include <bitcoin/node/transaction_indexer.hpp>

namespace libbitcoin {
namespace node {

/**
 * A full node on the Bitcoin P2P network.
 */
class BCN_API fullnode
{
public:
    /**
     * Construct the node.
     * The prefix must have been initialized using 'initchain' prior to this.
     * param@ [in]  directory  The path to the database folder.
     */
    fullnode(const std::string& directory);
    
    /**
     * Start the node.
     * @return  True if the start is initially successful.
     */
    bool start();

    /**
     * Stop the node.
     * Should only be called from the main thread.
     * It's an error to join() a thread from inside it.
     */
    void stop();

    /**
     * Blockchain accessor.
     */
    chain::blockchain& chain();

    /**
     * Transaction indexer accessor.
     */
    transaction_indexer& indexer();

private:
    void handle_start(const std::error_code& code);

    // New connection has been started.
    // Subscribe to new transaction messages from the network.
    void connection_started(const std::error_code& code,
        bc::network::channel_ptr node);

    // New transaction message from the network.
    // Attempt to validate it by storing it in the transaction pool.
    void recieve_tx(const std::error_code& ec, const transaction_type& tx,
        bc::network::channel_ptr node);

    // Result of store operation in transaction pool.
    void new_unconfirm_valid_tx(const std::error_code& code,
        const index_list& unconfirmed, const transaction_type& tx);

    threadpool net_pool_;
    threadpool disk_pool_;
    threadpool mem_pool_;
    bc::network::hosts peers_;
    bc::network::handshake handshake_;
    bc::network::network network_;
    bc::network::protocol protocol_;
    chain::blockchain_impl chain_;
    node::poller poller_;
    chain::transaction_pool txpool_;
    node::transaction_indexer txidx_;
    node::session session_;
};

} // namspace node
} //namespace libbitcoin

#endif