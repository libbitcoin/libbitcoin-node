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
#include <bitcoin/node/p2p_node.hpp>

#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/session_sync.hpp>

namespace libbitcoin {
namespace node {

#define LOG_NODE "LOG_P2P_NODE"

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::config;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

static const configuration default_configuration()
{
    configuration defaults;
    defaults.node.threads = NODE_THREADS;
    defaults.node.quorum = 2;
    defaults.node.headers_per_second = 18000;
    defaults.node.transaction_pool_capacity = NODE_TRANSACTION_POOL_CAPACITY;
    defaults.node.peers = NODE_PEERS;
    defaults.chain.threads = BLOCKCHAIN_THREADS;
    defaults.chain.block_pool_capacity = BLOCKCHAIN_BLOCK_POOL_CAPACITY;
    defaults.chain.history_start_height = BLOCKCHAIN_HISTORY_START_HEIGHT;
    defaults.chain.use_testnet_rules = BLOCKCHAIN_TESTNET_RULES_TESTNET;
    defaults.chain.database_path = boost::filesystem::path("s:\\node\\blockchain");
    defaults.chain.checkpoints = BLOCKCHAIN_CHECKPOINTS_TESTNET;
    defaults.network.threads = 1;
    defaults.network.identifier = NETWORK_IDENTIFIER_TESTNET;
    defaults.network.inbound_port = 0;
    defaults.network.inbound_connection_limit = 0;
    defaults.network.outbound_connections = 1;
    defaults.network.connect_attempts = NETWORK_CONNECT_ATTEMPTS;
    defaults.network.connect_timeout_seconds = 5;
    defaults.network.channel_handshake_seconds = NETWORK_CHANNEL_HANDSHAKE_SECONDS;
    defaults.network.channel_revival_minutes = NETWORK_CHANNEL_REVIVAL_MINUTES;
    defaults.network.channel_heartbeat_minutes = NETWORK_CHANNEL_HEARTBEAT_MINUTES;
    defaults.network.channel_inactivity_minutes = NETWORK_CHANNEL_INACTIVITY_MINUTES;
    defaults.network.channel_expiration_minutes = NETWORK_CHANNEL_EXPIRATION_MINUTES;
    defaults.network.channel_germination_seconds = NETWORK_CHANNEL_GERMINATION_SECONDS;
    defaults.network.host_pool_capacity = NETWORK_HOST_POOL_CAPACITY;
    defaults.network.relay_transactions = NETWORK_RELAY_TRANSACTIONS;
    defaults.network.hosts_file = boost::filesystem::path("s:\\node\\bn-hosts.cache");
    defaults.network.debug_file = boost::filesystem::path("s:\\node\\bn-debug.log");
    defaults.network.error_file = boost::filesystem::path("s:\\node\\bn-error.log");;
    defaults.network.self = NETWORK_SELF;
    defaults.network.blacklists = NETWORK_BLACKLISTS;
    defaults.network.seeds = NETWORK_SEEDS_TESTNET;
    return defaults;
};

const configuration p2p_node::defaults = default_configuration();

p2p_node::p2p_node(const configuration& configuration)
  : p2p(configuration.network),
    configuration_(configuration),
    blockchain_pool_(0),
    blockchain_(blockchain_pool_, configuration.chain)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void p2p_node::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    // TODO: move threadpool into blockchain start and make restartable.
    blockchain_pool_.join();
    blockchain_pool_.spawn(configuration_.chain.threads, thread_priority::low);

    blockchain_.start(
        std::bind(&p2p_node::handle_blockchain_start,
            this, _1, handler));
}

void p2p_node::handle_blockchain_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::info(LOG_NODE)
            << "Blockchain failed to start: " << ec.message();
        handler(ec);
        return;
    }

    blockchain_.fetch_last_height(
        std::bind(&p2p_node::handle_fetch_height,
            this, _1, _2, handler));
}

void p2p_node::handle_fetch_height(const code& ec, uint64_t height,
    result_handler handler)
{
    if (ec)
    {
        log::error(LOG_SESSION)
            << "Failure fetching blockchain start height: " << ec.message();
        handler(ec);
        return;
    }

    set_height(height);

    // This is the end of the derived start sequence.
    // Stopped is true and no network threads until after this call.
    p2p::start(handler);
}

// Run sequence.
// ----------------------------------------------------------------------------

void p2p_node::run(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    blockchain_.fetch_block_header(height(),
        std::bind(&p2p_node::handle_fetch_header,
            this, _1, _2, handler));
}

void p2p_node::handle_fetch_header(const code& ec, const header& header,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_SESSION)
            << "Failure fetching blockchain start header: " << ec.message();
        handler(ec);
        return;
    }

    const checkpoint start_block(header.hash() , height());

    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_sync>(start_block, configuration_)->start(
        dispatch_.ordered_delegate(&p2p_node::handle_synchronized,
            this, _1, handler));
}

void p2p_node::handle_synchronized(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Failure synchronizing blockchain: " << ec.message();
        handler(ec);
        return;
    }

    log::info(LOG_NETWORK)
        << "Completed synchronizing blockchain to height [" << height() << "]";

    // This is the end of the derived run sequence.
    p2p::run(handler);
}

// Stop sequence.
// ----------------------------------------------------------------------------

void p2p_node::stop(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    blockchain_pool_.shutdown();

    // This is the end of the derived stop sequence.
    p2p::stop(handler);
}

// Destruct sequence.
// ----------------------------------------------------------------------------

p2p_node::~p2p_node()
{
    close();
}

void p2p_node::close()
{
    stop([](code){});
    blockchain_pool_.join();

    // This is the end of the destruction sequence.
    p2p::close();
}

} // namspace node
} //namespace libbitcoin
