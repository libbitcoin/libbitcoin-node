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
using namespace boost::filesystem;
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::network;

static const configuration default_configuration()
{
    configuration defaults;
    defaults.node.quorum = NODE_QUORUM;
    defaults.node.threads = NODE_THREADS;
    defaults.node.blocks_per_second = NODE_BLOCKS_PER_SECOND;
    defaults.node.headers_per_second = NODE_HEADERS_PER_SECOND;
    defaults.node.transaction_pool_capacity = NODE_TRANSACTION_POOL_CAPACITY;
    defaults.node.transaction_pool_consistency = NODE_TRANSACTION_POOL_CONSISTENCY;
    defaults.node.peers = NODE_PEERS;
    defaults.chain.threads = BLOCKCHAIN_THREADS;
    defaults.chain.block_pool_capacity = BLOCKCHAIN_BLOCK_POOL_CAPACITY;
    defaults.chain.history_start_height = BLOCKCHAIN_HISTORY_START_HEIGHT;
    defaults.chain.use_testnet_rules = BLOCKCHAIN_TESTNET_RULES_TESTNET;
    defaults.chain.database_path = BLOCKCHAIN_DATABASE_PATH;
    defaults.chain.checkpoints = BLOCKCHAIN_CHECKPOINTS_TESTNET;
    defaults.network.threads = NETWORK_THREADS;
    defaults.network.identifier = NETWORK_IDENTIFIER_TESTNET;
    defaults.network.inbound_port = NETWORK_INBOUND_PORT_TESTNET;
    defaults.network.connection_limit = NETWORK_CONNECTION_LIMIT;
    defaults.network.outbound_connections = NETWORK_OUTBOUND_CONNECTIONS;
    defaults.network.manual_retry_limit = NETWORK_MANUAL_RETRY_LIMIT;
    defaults.network.connect_batch_size = NETWORK_CONNECT_BATCH_SIZE;
    defaults.network.connect_timeout_seconds = NETWORK_CONNECT_TIMEOUT_SECONDS;
    defaults.network.channel_handshake_seconds = NETWORK_CHANNEL_HANDSHAKE_SECONDS;
    defaults.network.channel_poll_seconds = NETWORK_CHANNEL_POLL_SECONDS;
    defaults.network.channel_heartbeat_minutes = NETWORK_CHANNEL_HEARTBEAT_MINUTES;
    defaults.network.channel_inactivity_minutes = NETWORK_CHANNEL_INACTIVITY_MINUTES;
    defaults.network.channel_expiration_minutes = NETWORK_CHANNEL_EXPIRATION_MINUTES;
    defaults.network.channel_germination_seconds = NETWORK_CHANNEL_GERMINATION_SECONDS;
    defaults.network.host_pool_capacity = NETWORK_HOST_POOL_CAPACITY;
    defaults.network.relay_transactions = NETWORK_RELAY_TRANSACTIONS;
    defaults.network.hosts_file = NETWORK_HOSTS_FILE;
    defaults.network.debug_file = NETWORK_DEBUG_FILE;
    defaults.network.error_file = NETWORK_ERROR_FILE;
    defaults.network.self = NETWORK_SELF;
    defaults.network.blacklists = NETWORK_BLACKLISTS;
    defaults.network.seeds = NETWORK_SEEDS_TESTNET;
    return defaults;
};

const configuration full_node::defaults = default_configuration();

constexpr auto append = std::ofstream::out | std::ofstream::app;

full_node::full_node(const configuration& config)
  : configuration_(config),
    debug_file_(config.network.debug_file.string(), append),
    error_file_(config.network.error_file.string(), append),
    database_threads_(config.chain.threads, thread_priority::low),
    blockchain_(database_threads_, config.chain),
    memory_threads_(config.node.threads, thread_priority::low),
    tx_pool_(memory_threads_, blockchain_,
        config.node.transaction_pool_capacity,
        config.node.transaction_pool_consistency),
    network_(config.network),
    node_threads_(config.network.threads, thread_priority::low),
    tx_indexer_(node_threads_),
    poller_(node_threads_, blockchain_),
    responder_(blockchain_, tx_pool_),
    session_(node_threads_, network_, blockchain_, poller_, tx_pool_,
        responder_, config.last_checkpoint_height())
{
}

full_node::~full_node()
{
    const auto unhandled = [](const code&){};
    stop(unhandled);
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

    static const auto startup = "================= startup ==================";
    log::debug(LOG_NODE) << startup;
    log::info(LOG_NODE) << startup;
    log::warning(LOG_NODE) << startup;
    log::error(LOG_NODE) << startup;
    log::fatal(LOG_NODE) << startup;

    blockchain_.start(
        std::bind(&full_node::handle_blockchain_start,
            this, _1, handler));
}

void full_node::handle_blockchain_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::info(LOG_NODE)
            << "Blockchain failed to start: " << ec.message();
        handler(ec);
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

    ////// Subscribe to new connections.
    ////network_.subscribe(
    ////    std::bind(&full_node::handle_new_channel,
    ////        this, _1, _2));

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
    session_.start();

    // This is just for logging, the blacklist is used directly from config.
    for (const auto& authority: configuration_.network.blacklists)
        log::info(LOG_NODE)
            << "Blacklisted peer [" << format(authority) << "]";

    // Start configured connections before starting the session.
    for (const auto& endpoint: configuration_.node.peers)
        network_.connect(endpoint.host(), endpoint.port(),
            std::bind(&full_node::handle_manual_connect,
                this, _1, _2, endpoint));

    handler(error::success);
}

std::string full_node::format(const config::authority& authority)
{
    auto formatted = authority.to_string();
    if (authority.port() == 0)
        formatted += ":*";

    return formatted;
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
// TODO: handle blockchain and network shutdown errors (file-based).
void full_node::stop(result_handler handler)
{
    code ec(error::success);

    node_threads_.shutdown();
    database_threads_.shutdown();
    memory_threads_.shutdown();
    tx_pool_.stop(/* handler */);
    blockchain_.stop(/* handler */);
    network_.stop([](code){});

    node_threads_.join();
    database_threads_.join();
    memory_threads_.join();
    network_.close();

    handler(ec);
}

bool full_node::handle_new_channel(const code& ec, channel::ptr node)
{
    if (ec == error::service_stopped)
        return false;

    if (ec)
    {
        log::info(LOG_NODE)
            << "Error starting transaction subscription for ["
            << node ->authority() << "] " << ec.message();
        return false;
    }

    // Subscribe to transaction messages from this channel.
    node->subscribe<transaction>(
        std::bind(&full_node::handle_recieve_tx,
            this, _1, _2, node));

    // Stay subscribed to new channels unless stopping.
    return true;
}

bool full_node::handle_recieve_tx(const code& ec, const transaction& tx,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    const auto hash = tx.hash();
    const auto encoded = encode_hash(hash);

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure receiving transaction [" << encoded << "] from ["
            << node->authority() << "] " << ec.message();
        return false;
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

    // Resubscribe to receive transaction messages from this channel.
    return true;
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

    log::debug(LOG_NODE)
        << "Accepted transaction [" << encoded
        << "] with unconfirmed input indexes (" << format(unconfirmed) << ")";

    tx_indexer_.index(tx, 
        std::bind(&full_node::handle_tx_indexed,
            this, _1, hash));
}

void full_node::handle_tx_indexed(const code& ec, const hash_digest& hash)
{
    const auto encoded = encode_hash(hash);

    if (ec)
        log::error(LOG_NODE)
            << "Failure adding transaction [" << encoded
            << "] to memory pool index: " << ec.message();
    else
        log::debug(LOG_NODE)
            << "Added transaction [" << encoded << "] to memory pool index.";
}

// HACK: this is for access to handle_new_blocks to facilitate server
// inheritance of full_node. The organization should be refactored.
bool full_node::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block_chain::list& new_blocks,
    const block_chain::list& replaced_blocks)
{
    return session_.handle_new_blocks(ec, fork_point, new_blocks,
        replaced_blocks);
}

} // namspace node
} //namespace libbitcoin
