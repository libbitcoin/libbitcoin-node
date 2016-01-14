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
#include <functional>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/indexer.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>
#include <bitcoin/node/session.hpp>

namespace libbitcoin {
namespace node {

#define LOG_NODE "node"

/**
 * A full node on the Bitcoin P2P network.
 */
class BCN_API full_node
{
public:
    typedef std::function<void(const code&)> result_handler;
    typedef std::function<void(const code&, network::channel::ptr)>
        channel_handler;

    static const configuration defaults;

    /**
     * Construct the node.
     * The prefix must have been initialized using 'initchain' prior to this.
     * param@ [in]  config  The configuration settings for the node.
     */
    full_node(const configuration& config=defaults);

    /**
     * Block until all threads are coalesced.
     */
    ~full_node();
    
    /**
     * Start the node.
     */
    virtual void start(result_handler handler);

    /**
     * Stop the node.
     * Must only be called from the main thread.
     */
    virtual void stop(result_handler handler);

    /// Accessors
    virtual blockchain::block_chain& blockchain();
    virtual blockchain::transaction_pool& transaction_pool();
    virtual node::indexer& transaction_indexer();
    virtual network::p2p& network();
    virtual threadpool& pool();

protected:
    /// New channel has been started.
    virtual bool handle_new_channel(const code& ec,
        network::channel::ptr node);

    /// New transaction has been received from the network.
    virtual bool handle_recieve_tx(const code& ec,
        const chain::transaction& tx, network::channel::ptr node);

    /// New transaction has been validated and accepted into the pool.
    virtual void handle_tx_validated(const code& ec,
        const chain::transaction& tx, const hash_digest& hash,
        const index_list& unconfirmed);

    /// New block(s) have been accepted into the chain.
    virtual bool handle_new_blocks(const code& ec, uint64_t fork_point,
        const blockchain::block_chain::list& new_blocks,
        const blockchain::block_chain::list& replaced_blocks);

    // These must be libbitcoin streams.
    bc::ofstream debug_file_;
    bc::ofstream error_file_;

    threadpool database_threads_;
    blockchain::blockchain_impl blockchain_;

    threadpool memory_threads_;
    blockchain::transaction_pool tx_pool_;

    // network_ manages its own threads, others will eventually
    network::p2p network_;

    threadpool node_threads_;
    node::indexer tx_indexer_;
    node::poller poller_;
    node::responder responder_;
    node::session session_;

private:
    static std::string format(const config::authority& authority);
    static std::string format(const index_list& unconfirmed);

    void handle_blockchain_start(const code& ec, result_handler handler);
    void handle_network_start(const code& ec, result_handler handler);
    void handle_fetch_height(const code& ec, uint64_t height,
        result_handler handler);
    void handle_manual_connect(const code& ec, network::channel::ptr channel,
        const config::endpoint& endpoint);
    void handle_tx_indexed(const code& ec, const hash_digest& hash);
    void handle_tx_deindexed(const code& ec, const hash_digest& hash);
    void handle_tx_confirmed(const code& ec, const chain::transaction& tx,
        const hash_digest& hash);

    const configuration configuration_;
};

} // namspace node
} //namespace libbitcoin

#endif
