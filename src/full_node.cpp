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
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/indexer.hpp>
#include <bitcoin/node/logging.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>
#include <bitcoin/node/session.hpp>

// Localizable messages.

// Session errors.
#define BN_CONNECTION_START_ERROR \
    "Error starting connection: %1%"
#define BN_SESSION_START_ERROR \
    "Error starting session: %1%"
#define BN_SESSION_STOP_ERROR \
    "Error stopping session: %1%"
#define BN_SESSION_START_HEIGHT_FETCH_ERROR \
    "Error fetching start height: %1%"
#define BN_SESSION_START_HEIGHT_SET_ERROR \
    "Error setting start height: %1%"
#define BN_SESSION_START_HEIGHT \
    "Set start height (%1%)"

// Transaction successes.
#define BN_TX_ACCEPTED \
    "Accepted transaction into memory pool [%1%]"
#define BN_TX_ACCEPTED_WITH_INPUTS \
    "Accepted transaction into memory pool [%1%] with unconfirmed inputs (%2%)"
#define BN_TX_CONFIRMED \
    "Confirmed transaction into blockchain [%1%]"

// Transaction failures.
#define BN_TX_ACCEPT_FAILURE \
    "Failure accepting transaction in memory pool [%1%] %2%"
#define BN_TX_CONFIRM_FAILURE \
    "Failure confirming transaction into blockchain [%1%] %2%"
#define BN_TX_DEINDEX_FAILURE \
    "Failure deindexing transaction [%1%] %2%"
#define BN_TX_INDEX_FAILURE \
    "Failure indexing transaction [%1%] %2%"
#define BN_TX_RECEIVE_FAILURE \
    "Failure receiving transaction [%1%] %2%"

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using boost::format;
using namespace boost::filesystem;
using namespace bc::network;

full_node::full_node(/* configuration */)
  : network_threads_(BN_THREADS_NETWORK, thread_priority::low),
    database_threads_(BN_THREADS_DISK, thread_priority::low),
    memory_threads_(BN_THREADS_MEMORY, thread_priority::low),
    host_pool_(network_threads_, BN_HOSTS_FILENAME, BN_P2P_HOST_POOL),
    handshake_(network_threads_, BN_LISTEN_PORT),
    network_(network_threads_),
    protocol_(network_threads_, host_pool_, handshake_, network_,
        protocol::default_seeds, BN_LISTEN_PORT, BN_P2P_OUTBOUND),
    blockchain_(database_threads_, BN_DIRECTORY, { BN_HISTORY_START_HEIGHT },
        BN_P2P_ORPHAN_POOL/*, BN_CHECKPOINT_HEIGHT*/),
    tx_pool_(memory_threads_, blockchain_, BN_P2P_TX_POOL),
    tx_indexer_(memory_threads_),
    poller_(memory_threads_, blockchain_),
    responder_(blockchain_, tx_pool_),
    session_(network_threads_, handshake_, protocol_, blockchain_, poller_,
        tx_pool_, responder_)
{
}

bool full_node::start()
{
    // Start the blockchain.
    if (!blockchain_.start())
        return false;

    std::promise<std::error_code> height_promise;
    blockchain_.fetch_last_height(
        std::bind(&full_node::set_height,
            this, _1, _2, std::ref(height_promise)));

    // Wait for set completion.
    auto result = !height_promise.get_future().get();
    if (!result)
        return false;

    // Start the transaction pool.
    tx_pool_.start();

    // TODO: include manually-configured endpoints from config.

    std::promise<std::error_code> session_promise;
    session_.start(
        std::bind(&full_node::handle_start,
            this, _1, std::ref(session_promise)));

    // Wait for start completion.
    return !session_promise.get_future().get();
}

bool full_node::stop()
{
    // Use promise to block on main thread until stop completes.
    std::promise<std::error_code> promise;

    // Stop the session.
    session_.stop(
        std::bind(&full_node::handle_stop,
            this, _1, std::ref(promise)));

    // Wait for stop completion.
    auto result = !promise.get_future().get();

    // Try and close blockchain database even if session stop failed.
    // Blockchain stop is currently non-blocking, so the result is misleading.
    if (!blockchain_.stop())
        result = false;

    // Stop threadpools.
    network_threads_.stop();
    database_threads_.stop();
    memory_threads_.stop();

    // Join threadpools. Wait for them to finish.
    network_threads_.join();
    database_threads_.join();
    memory_threads_.join();

    return result;
}

bc::blockchain::blockchain& full_node::blockchain()
{
    return blockchain_;
}

bc::blockchain::transaction_pool& full_node::transaction_pool()
{
    return tx_pool_;
}

bc::node::indexer& full_node::transaction_indexer()
{
    return tx_indexer_;
}

bc::network::protocol& full_node::protocol()
{
    return protocol_;
}

bc::threadpool& full_node::threadpool()
{
    return memory_threads_;
}

void full_node::handle_start(const std::error_code& ec,
    std::promise<std::error_code>& promise)
{
    if (ec)
    {
        log_error(LOG_NODE)
            << format(BN_SESSION_START_ERROR) % ec.message();
        promise.set_value(ec);
        return;
    }

    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&full_node::new_channel,
            this, _1, _2));

    promise.set_value(ec);
}

void full_node::set_height(const std::error_code& ec, uint64_t height,
    std::promise<std::error_code>& promise)
{
    if (ec)
    {
        log_error(LOG_SESSION)
            << format(BN_SESSION_START_HEIGHT_FETCH_ERROR) % ec.message();
        promise.set_value(ec);
        return;
    }

    const auto handle_set_height = [height, &promise]
        (const std::error_code& ec)
    {
        if (ec)
            log_error(LOG_SESSION)
                << format(BN_SESSION_START_HEIGHT_SET_ERROR) % ec.message();
        else
            log_info(LOG_SESSION)
                << format(BN_SESSION_START_HEIGHT) % height;

        promise.set_value(ec);
    };

    handshake_.set_start_height(height, handle_set_height);
}

void full_node::handle_stop(const std::error_code& ec, 
    std::promise<std::error_code>& promise)
{
    if (ec)
        log_error(LOG_NODE)
            << format(BN_SESSION_STOP_ERROR) % ec.message();

    promise.set_value(ec);
}

void full_node::new_channel(const std::error_code& ec, channel::pointer node)
{
    if (!node || ec == bc::error::service_stopped)
        return;

    if (ec)
    {
        log_info(LOG_NODE)
            << format(BN_CONNECTION_START_ERROR) % ec.message();
        return;
    }

    // Subscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&full_node::recieve_tx,
            this, _1, _2, node));

    // Stay subscribed to new connections.
    protocol_.subscribe_channel(
        std::bind(&full_node::new_channel,
            this, _1, _2));
}

void full_node::recieve_tx(const std::error_code& ec,
    const chain::transaction& tx, channel::pointer node)
{
    if (ec)
    {
        const auto hash = encode_hash(tx.hash());
        log_debug(LOG_NODE)
            << format(BN_TX_RECEIVE_FAILURE) % hash % ec.message();
        return;
    }

    // Called when the transaction becomes confirmed in a block.
    const auto handle_confirm = [this, tx](const std::error_code& ec)
    {
        const auto hash = encode_hash(tx.hash());

        if (ec)
            log_warning(LOG_NODE)
            << format(BN_TX_CONFIRM_FAILURE) % hash % ec.message();
        else
            log_debug(LOG_NODE)
                << format(BN_TX_CONFIRMED) % hash;

        const auto handle_deindex = [hash](const std::error_code& ec)
        {
            if (ec)
                log_error(LOG_NODE)
                    << format(BN_TX_DEINDEX_FAILURE) % hash % ec.message();
        };

        tx_indexer_.deindex(tx, handle_deindex);
    };

    // Validate and store the tx in the transaction mempool.
    tx_pool_.store(tx, handle_confirm,
        std::bind(&full_node::new_unconfirm_valid_tx,
            this, _1, _2, tx));

    // Resubscribe to receive transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&full_node::recieve_tx,
            this, _1, _2, node));
}

static std::string format_unconfirmed_inputs(
    const chain::index_list& unconfirmed)
{
    if (unconfirmed.empty())
        return "";

    std::vector<std::string> inputs;
    for (const auto input: unconfirmed)
        inputs.push_back(boost::lexical_cast<std::string>(input));

    return bc::join(inputs, ",");
}

void full_node::new_unconfirm_valid_tx(const std::error_code& ec,
    const chain::index_list& unconfirmed, const chain::transaction& tx)
{
    const auto hash = encode_hash(tx.hash());

    const auto handle_index = [hash](const std::error_code& ec)
    {
        if (ec)
            log_error(LOG_NODE)
                << format(BN_TX_INDEX_FAILURE) % hash % ec.message();
    };

    if (ec)
        log_debug(LOG_NODE)
            << format(BN_TX_ACCEPT_FAILURE) % hash % ec.message();
    else if (unconfirmed.empty())
        log_debug(LOG_NODE)
            << format(BN_TX_ACCEPTED) % hash;
    else
        log_debug(LOG_NODE)
            << format(BN_TX_ACCEPTED_WITH_INPUTS) % hash %
                format_unconfirmed_inputs(unconfirmed);
        
    if (!ec)
        tx_indexer_.index(tx, handle_index);
}

// HACK: this is for access to broadcast_new_blocks to facilitate server
// inheritance of full_node. The organization should be refactored.
void full_node::broadcast_new_blocks(const std::error_code& ec,
    uint32_t fork_point, const blockchain::blockchain::block_list& new_blocks,
    const blockchain::blockchain::block_list& replaced_blocks)
{
    session_.broadcast_new_blocks(ec, fork_point, new_blocks, replaced_blocks);
}

} // namspace node
} //namespace libbitcoin
