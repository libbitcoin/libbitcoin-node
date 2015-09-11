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
#include <bitcoin/node/config/settings_type.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/indexer.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>
#include <bitcoin/node/session.hpp>

namespace libbitcoin {
namespace node {

// Configuration setting defaults.

// [node]
#define NODE_THREADS                        4
#define NODE_TRANSACTION_POOL_CAPACITY      2000
#define NODE_PEERS                          {}
#define NODE_BLACKLISTS                     {}

// [blockchain]
#define BLOCKCHAIN_THREADS                  6
#define BLOCKCHAIN_BLOCK_POOL_CAPACITY      50
#define BLOCKCHAIN_HISTORY_START_HEIGHT     0
#define BLOCKCHAIN_DATABASE_PATH            boost::filesystem::path("blockchain")
#define BLOCKCHAIN_CHECKPOINTS              bc::blockchain::checkpoint::defaults

// [network]
#define NETWORK_THREADS                     4
#define NETWORK_INBOUND_PORT                protocol_port
#define NETWORK_INBOUND_CONNECTION_LIMIT    8
#define NETWORK_OUTBOUND_CONNECTIONS        8
#define NETWORK_CONNECT_TIMEOUT_SECONDS     5
#define NETWORK_CHANNEL_HANDSHAKE_SECONDS   30
#define NETWORK_CHANNEL_REVIVAL_MINUTES     5
#define NETWORK_CHANNEL_HEARTBEAT_MINUTES   5
#define NETWORK_CHANNEL_INACTIVITY_MINUTES  30
#define NETWORK_CHANNEL_EXPIRATION_MINUTES  90
#define NETWORK_CHANNEL_GERMINATION_SECONDS 45
#define NETWORK_HOST_POOL_CAPACITY          1000
#define NETWORK_RELAY_TRANSACTIONS          true
#define NETWORK_HOSTS_FILE                  boost::filesystem::path("hosts.cache")
#define NETWORK_DEBUG_FILE                  boost::filesystem::path("debug.log")
#define NETWORK_ERROR_FILE                  boost::filesystem::path("error.log")
#define NETWORK_SELF                        bc::unspecified_network_address
#define NETWORK_SEEDS                       network::seeder::defaults

/**
 * A full node on the Bitcoin P2P network.
 */
class BCN_API full_node
{
public:
    static const settings_type defaults;

    /**
     * Construct the node.
     * The prefix must have been initialized using 'initchain' prior to this.
     * param@ [in]  config  The configuration settings for the node.
     */
    full_node(const settings_type& config=defaults);
    
    /**
     * Start the node.
     * param@ [in]  config  The configuration settings for the node.
     * @return  True if the start is successful.
     */
    virtual bool start(const settings_type& config=defaults);

    /**
     * Stop the node.
     * Should only be called from the main thread.
     * It's an error to join() a thread from inside it.
     * @return  True if the stop is successful.
     */
    virtual bool stop();

    // Accessors
    virtual bc::blockchain::blockchain& blockchain();
    virtual bc::blockchain::transaction_pool& transaction_pool();
    virtual node::indexer& transaction_indexer();
    virtual network::protocol& protocol();
    virtual threadpool& pool();

protected:
    // Result of store operation in transaction pool.
    virtual void new_unconfirm_valid_tx(const code& code,
        const chain::index_list& unconfirmed, const chain::transaction& tx);

    // New channel has been started.
    // Subscribe to new transaction messages from the network.
    virtual void new_channel(const code& ec,
        network::channel::ptr node);

    // New transaction message from the network.
    // Attempt to validate it by storing it in the transaction pool.
    virtual void recieve_tx(const code& ec,
        const chain::transaction& tx, network::channel::ptr node);

    // HACK: this is for access to broadcast_new_blocks to facilitate server
    // inheritance of full_node. The organization should be refactored.
    virtual void broadcast_new_blocks(const code& ec,
        uint32_t fork_point,
        const bc::blockchain::blockchain::block_list& new_blocks,
        const bc::blockchain::blockchain::block_list& replaced_blocks);

    // These must be bc types.
    bc::ofstream debug_file_;
    bc::ofstream error_file_;

    threadpool network_threads_;
    network::hosts host_pool_;
    network::initiator network_;
    network::protocol protocol_;

    threadpool database_threads_;
    bc::blockchain::blockchain_impl blockchain_;

    threadpool memory_threads_;
    bc::blockchain::transaction_pool tx_pool_;
    node::indexer tx_indexer_;
    node::poller poller_;
    node::responder responder_;
    node::session session_;

private:
    void handle_start(const code& ec,
        std::promise<code>& promise);
    void handle_stop(const code& ec,
        std::promise<code>& promise);
    void set_height(const code& ec, uint64_t height,
        std::promise<code>& promise);
};

} // namspace node
} //namespace libbitcoin

#endif
