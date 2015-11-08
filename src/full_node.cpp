/*
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
#include <bitcoin/node/full_node.hpp>

#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/indexer.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>
#include <bitcoin/node/session.hpp>

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using boost::posix_time::seconds;
using boost::posix_time::minutes;
using namespace boost::filesystem;
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::network;

// Beware of static initializer ordering here.
const configuration full_node::defaults = configuration
{
    // [node]
    node::settings
    {
        2,
        NODE_TRANSACTION_POOL_CAPACITY,
        NODE_PEERS,
        NODE_BLACKLISTS
    },

    // [blockchain]
    blockchain::settings
    {
        2,
        BLOCKCHAIN_BLOCK_POOL_CAPACITY,
        BLOCKCHAIN_HISTORY_START_HEIGHT,
        true,
        BLOCKCHAIN_DATABASE_PATH,
        bc::blockchain::checkpoint::testnet
    },

    // [network]
    network::p2p::testnet
};

constexpr auto append = std::ofstream::out | std::ofstream::app;

/* TODO: create a configuration class for thread priority. */
full_node::full_node(const configuration& config)
  : configuration_(config),
    debug_file_(config.network.debug_file.string(), append),
    error_file_(config.network.error_file.string(), append),

    database_threads_(config.chain.threads, thread_priority::low),
    blockchain_(
        database_threads_,
        config.chain.database_path.string(),
        config.chain.history_start_height,
        config.chain.block_pool_capacity,
        config.chain.use_testnet_rules,
        config.chain.checkpoints),

    memory_threads_(config.node.threads, thread_priority::low),
    tx_pool_(memory_threads_, blockchain_,
        config.node.transaction_pool_capacity),
        
    /* node threads is now duplicative as network_ manages its own threads */
    node_threads_(config.network.threads, thread_priority::low),
    network_(config.network),
    tx_indexer_(node_threads_),
    poller_(node_threads_, blockchain_),
    responder_(blockchain_, tx_pool_),
    session_(
        node_threads_,
        network_,
        blockchain_,
        poller_,
        tx_pool_,
        responder_, 
        config.minimum_start_height())
{
}

block_chain& full_node::blockchain()
{
    return blockchain_;
}

transaction_pool& full_node::transaction_pool()
{
    return tx_pool_;
}

node::indexer& full_node::transaction_indexer()
{
    return tx_indexer_;
}

p2p& full_node::network()
{
    return network_;
}

threadpool& full_node::pool()
{
    return memory_threads_;
}

void full_node::start(result_handler handler)
{
    initialize_logging(debug_file_, error_file_, bc::cout, bc::cerr);

    if (!blockchain_.start())
    {
        log::info(LOG_NODE)
            << "Blockchain failed to start.";
        handler(error::operation_failed);
        return;
    }

    tx_pool_.start();

    network_.start(
        std::bind(&full_node::handle_network_start,
            this, _1, handler));
}

void full_node::handle_network_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::error(LOG_NODE)
            << "Error starting session: " << ec.message();
        handler(ec);
        return;
    }

    // Subscribe to new connections.
    network_.subscribe(
        std::bind(&full_node::handle_new_channel,
            this, _1, _2));

    // Pass the initial blockchain height to the network.
    blockchain_.fetch_last_height(
        std::bind(&full_node::handle_fetch_height,
            this, _1, _2, handler));
}

void full_node::handle_fetch_height(const code& ec, uint64_t height,
    result_handler handler)
{
    if (ec)
    {
        log::error(LOG_SESSION)
            << "Error fetching start height: " << ec.message();
        handler(ec);
        return;
    }

    network_.set_height(height);

    session_.start(
        std::bind(&full_node::handle_session_start,
            this, _1, handler));
}

std::string full_node::format(const config::authority& authority)
{
    auto formatted = authority.to_string();
    if (authority.port() == 0)
        formatted += ":*";

    return formatted;
}

void full_node::handle_session_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::error(LOG_SESSION)
            << "Error starting session: " << ec.message();
        handler(ec);
        return;
    }

    // This is just for logging, the blacklist is used directly from config.
    for (const auto& authority: configuration_.node.blacklists)
    {
        log::info(LOG_NODE)
            << "Blacklisted peer [" << format(authority) << "]";
    }

    // Start configured connections before starting the session.
    for (const auto& endpoint: configuration_.node.peers)
    {
        network_.connect(endpoint.host(), endpoint.port(),
            std::bind(&full_node::handle_manual_connect,
                this, _1, _2, endpoint));
    }

    handler(error::success);
}

// This will log on the first successful connection to each configured peer.
void full_node::handle_manual_connect(const code& ec, channel::ptr channel,
    const config::endpoint& endpoint)
{
    if (ec)
        log::info(LOG_NODE)
            << "Failure connecting configured peer [" << endpoint << "] "
            << ec.message();
    else
        log::info(LOG_NODE)
            << "Connected configured peer [" << endpoint << "]";
}

// The handler is not called until all threads are coalesced.
void full_node::stop(result_handler handler)
{
    session_.stop(
        std::bind(&full_node::handle_session_stop,
            this, _1, handler));
}

// TODO: bury thread management in blockchain and tx pool (see network).
void full_node::handle_session_stop(const code& ec, result_handler handler)
{
    if (ec)
        log::error(LOG_NODE)
            << "Error stopping session: " << ec.message();
    else
        log::debug(LOG_NODE)
            << "Session stopped.";

    if (!blockchain_.stop())
        log::error(LOG_NODE)
            << "Error stopping blockchain.";
    else
        log::debug(LOG_NODE)
            << "Blockchain stopped.";

    node_threads_.shutdown();
    database_threads_.shutdown();
    memory_threads_.shutdown();
    tx_pool_.stop();
    network_.stop();
    log::debug(LOG_NODE)
        << "Threads signaled.";

    node_threads_.join();
    database_threads_.join();
    memory_threads_.join();
    network_.close();
    log::debug(LOG_NODE)
        << "Threads joined.";

    handler(ec);
}

void full_node::handle_new_channel(const code& ec, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    // Stay subscribed to new channels unless stopping.
    network_.subscribe(
        std::bind(&full_node::handle_new_channel,
            this, _1, _2));

    if (ec)
    {
        log::info(LOG_NODE)
            << "Error starting transaction subscription for ["
            << node ->authority() << "] " << ec.message();
        return;
    }

    // Subscribe to transaction messages from this channel.
    node->subscribe<transaction>(
        std::bind(&full_node::handle_recieve_tx,
            this, _1, _2, node));
}

void full_node::handle_recieve_tx(const code& ec, const transaction& tx,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    // Resubscribe to receive transaction messages from this channel.
    node->subscribe<transaction>(
        std::bind(&full_node::handle_recieve_tx,
            this, _1, _2, node));

    const auto hash = tx.hash();
    const auto encoded = encode_hash(hash);

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure receiving transaction [" << encoded << "] from ["
            << node->authority() << "] " << ec.message();
        return;
    }

    log::debug(LOG_NODE)
        << "Received transaction [" << encoded << "] from ["
        << node->authority() << "]";

    // Validate the tx and store it in the memory pool.
    // If validation returns an error then confirmation will never be called.
    tx_pool_.store(tx, 
        std::bind(&full_node::handle_tx_confirmed,
            this, _1, _2, _3),
        std::bind(&full_node::handle_tx_validated,
            this, _1, _2, _3, _4));
}

// Called when the transaction becomes confirmed in a block.
void full_node::handle_tx_confirmed(const code& ec, const transaction& tx,
    const hash_digest& hash)
{
    const auto encoded = encode_hash(hash);

    // This could be error::blockchain_reorganized (we dump the pool),
    // error::pool_filled or error::double_spend (where the tx is a dependency
    // of retractive double spend).
    if (ec)
        log::warning(LOG_NODE)
        << "Discarded transaction [" << encoded << "] from memory pool: "
            << ec.message();
    else
        log::debug(LOG_NODE)
        << "Confirmed transaction [" << encoded << "] into blockchain.";

    tx_indexer_.deindex(tx,
        std::bind(&full_node::handle_tx_deindexed,
            this, _1, hash));
}

void full_node::handle_tx_deindexed(const code& ec, const hash_digest& hash)
{
    const auto encoded = encode_hash(hash);

    if (ec)
        log::error(LOG_NODE)
            << "Failure removing confirmed transaction [" << encoded
            << "] from memory pool index: " << ec.message();
    else
        log::debug(LOG_NODE)
            << "Removed confirmed transaction [" << encoded
            << "] from memory pool index.";
}

std::string full_node::format(const index_list& unconfirmed)
{
    if (unconfirmed.empty())
        return "";

    std::vector<std::string> inputs;
    for (const auto input: unconfirmed)
        inputs.push_back(boost::lexical_cast<std::string>(input));

    return bc::join(inputs, ",");
}

// Called when the transaction passes memory pool validation.
void full_node::handle_tx_validated(const code& ec, const transaction& tx,
    const hash_digest& hash, const index_list& unconfirmed)
{
    const auto encoded = encode_hash(hash);

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure accepting transaction [" << encoded << "]"
            << ec.message();
        return;
    }

    if (unconfirmed.empty())
        log::debug(LOG_NODE)
            << "Accepted transaction [" << encoded << "]";
    else
        log::debug(LOG_NODE)
            << "Accepted transaction [" << encoded
            << "] with unconfirmed inputs (" << format(unconfirmed) << ")";

    tx_indexer_.index(tx, 
        std::bind(&full_node::handle_tx_indexed,
            this, _1, hash));
}

void full_node::handle_tx_indexed(const code& ec, const hash_digest& hash)
{
    const auto encoded = encode_hash(hash);

    if (ec)
        log::error(LOG_NODE)
            << "Failure iadding transaction [" << encoded
            << "] to memory pool index: " << ec.message();
    else
        log::error(LOG_NODE)
            << "Added transaction [" << encoded << "] to memory pool index.";
}

// HACK: this is for access to handle_new_blocks to facilitate server
// inheritance of full_node. The organization should be refactored.
void full_node::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block_chain::list& new_blocks,
    const block_chain::list& replaced_blocks)
{
    session_.handle_new_blocks(ec, fork_point, new_blocks, replaced_blocks);
}

} // namspace node
} //namespace libbitcoin
