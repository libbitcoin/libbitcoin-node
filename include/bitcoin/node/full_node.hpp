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
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <cstdint>
#include <future>
#include <string>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/config/settings.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/indexer.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>
#include <bitcoin/node/session.hpp>

namespace libbitcoin {
namespace node {

// Configuration parameters.
// TODO: incorporate channel timeouts.
#define BN_DATABASE_THREADS         6
#define BN_NETWORK_THREADS          4
#define BN_MEMORY_THREADS           4
#define BN_HOST_POOL_CAPACITY       1000
#define BN_BLOCK_POOL_CAPACITY      50
#define BN_TX_POOL_CAPACITY         2000
#define BN_HISTORY_START_HEIGHT     0
#define BN_CHECKPOINT_HEIGHT        0
#define BN_CHECKPOINT_HASH          bc::null_hash
#define BN_P2P_INBOUND_PORT         bc::protocol_port
#define BN_P2P_OUTBOUND_CONNECTIONS 8
#define BN_P2P_HOSTS_FILE           "peers"
#define BN_BLOCKCHAIN_DIRECTORY     "blockchain"

/**
 * A full node on the Bitcoin P2P network.
 */
class BCN_API full_node
{
public:
    /**
     * Construct the node using default configuration.
     * The prefix must have been initialized using 'initchain' prior to this.
     */
    full_node();

    /**
     * Construct the node.
     * The prefix must have been initialized using 'initchain' prior to this.
     * param@ [in]  configuration  The configuration settings for the node.
     */
    full_node(const settings& configuration);
    
    /**
     * Start the node.
     * @return  True if the start is successful.
     */
    virtual bool start();

    /**
     * Stop the node.
     * Should only be called from the main thread.
     * It's an error to join() a thread from inside it.
     * @return  True if the stop is successful.
     */
    virtual bool stop();

    // Accessors
    virtual bc::chain::blockchain& blockchain();
    virtual bc::chain::transaction_pool& transaction_pool();
    virtual bc::node::indexer& transaction_indexer();
    virtual bc::network::protocol& protocol();
    virtual bc::threadpool& threadpool();

protected:
    // Result of store operation in transaction pool.
    virtual void new_unconfirm_valid_tx(const std::error_code& code,
        const index_list& unconfirmed, const transaction_type& tx);

    // New channel has been started.
    // Subscribe to new transaction messages from the network.
    virtual void new_channel(const std::error_code& ec,
        bc::network::channel_ptr node);

    // New transaction message from the network.
    // Attempt to validate it by storing it in the transaction pool.
    virtual void recieve_tx(const std::error_code& ec,
        const transaction_type& tx, bc::network::channel_ptr node);

    // HACK: this is for access to broadcast_new_blocks to facilitate server
    // inheritance of full_node. The organization should be refactored.
    virtual void broadcast_new_blocks(const std::error_code& ec,
        uint32_t fork_point, const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list& replaced_blocks);

    bc::threadpool network_threads_;
    bc::threadpool database_threads_;
    bc::threadpool memory_threads_;
    bc::network::hosts host_pool_;
    bc::network::handshake handshake_;
    bc::network::network network_;
    bc::network::protocol protocol_;
    bc::chain::blockchain_impl blockchain_;
    bc::chain::transaction_pool tx_pool_;
    bc::node::indexer tx_indexer_;
    bc::node::poller poller_;
    bc::node::session session_;
    bc::node::responder responder_;

private:
    void handle_start(const std::error_code& ec,
        std::promise<std::error_code>& promise);
    void handle_stop(const std::error_code& ec,
        std::promise<std::error_code>& promise);
    void set_height(const std::error_code& ec, uint64_t height,
        std::promise<std::error_code>& promise);
};

} // namspace node
} //namespace libbitcoin

#endif
