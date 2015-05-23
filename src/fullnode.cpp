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
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/fullnode.hpp>

#define LOG_NODE "node"

// Localizable messages.
#define BN_ACCEPTED_TRANSACTION \
    "Accepted transaction [%1%]%2%"
#define BN_INDEX_ERROR \
    "Index error : %1%"
#define BN_CONFIRM_TX \
    "Confirm transaction [%1%]"
#define BN_CONFIRM_TX_ERROR \
    "Confirm transaction error [%1%] : %2%"
#define BN_CONNECTION_START_ERROR \
    "Connection start error : %1%"
#define BN_DEINDEX_TX_ERROR \
    "Deindex error : %1%"
#define BN_MEMPOOL_ERROR \
    "Error storing memory pool transaction [%1%] : %2%"
#define BN_RECEIVE_TX_ERROR \
    "Receive transaction error [%1%] : %2%"
#define BN_SESSION_STOP_ERROR \
    "Problem stopping session : %1%"
#define BN_START_ERROR \
    "Node start error : %1%"
#define BN_WITH_UNCONFIRMED_INPUTS \
    "with unconfirmed inputs (%1%)"

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using boost::format;
using namespace boost::filesystem;
using namespace bc::network;

fullnode::fullnode(/* configuration */)
  : net_pool_(BN_THREADS_NETWORK, thread_priority::normal),
    disk_pool_(BN_THREADS_DISK, thread_priority::low),
    mem_pool_(BN_THREADS_MEMORY, thread_priority::low),
    peers_(net_pool_, BN_HOSTS_FILENAME, BN_P2P_HOSTS),
    handshake_(net_pool_),
    network_(net_pool_),
    protocol_(net_pool_, peers_, handshake_, network_, BN_P2P_CONNECTIONS),
    chain_(disk_pool_, BN_DIRECTORY, { BN_HISTORY_START }, BN_P2P_ORPHAN_POOL),
    poller_(mem_pool_, chain_),
    txpool_(mem_pool_, chain_, BN_P2P_TX_POOL),
    txidx_(mem_pool_),
    session_(net_pool_, handshake_, protocol_, chain_, poller_, txpool_ )
{
}
 
bool fullnode::start()
{
    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&fullnode::connection_started, this, _1, _2));

    // Start blockchain
    if (!chain_.start())
        return false;

    // Start transaction pool
    txpool_.start();

    // Fire off app.
    const auto handle_start = std::bind(&fullnode::handle_start, this, _1);
    session_.start(handle_start);
    return true;
}

void fullnode::stop()
{
    std::promise<std::error_code> code_promise;
    const auto session_stopped = [&code_promise](const std::error_code& code)
    {
        code_promise.set_value(code);
    };

    session_.stop(session_stopped);
    const auto code = code_promise.get_future().get();
    if (code)
        log_error(LOG_NODE) << format(BN_SESSION_STOP_ERROR) % code.message();

    // Safely close blockchain database.
    chain_.stop();

    // Stop threadpools.
    net_pool_.stop();
    disk_pool_.stop();
    mem_pool_.stop();

    // Join threadpools. Wait for them to finish.
    net_pool_.join();
    disk_pool_.join();
    mem_pool_.join();
}

chain::blockchain& fullnode::chain()
{
    return chain_;
}

transaction_indexer& fullnode::indexer()
{
    return txidx_;
}

void fullnode::handle_start(const std::error_code& code)
{
    if (code)
        log_error(LOG_NODE) << format(BN_START_ERROR) % code.message();
}

void fullnode::connection_started(const std::error_code& code,
    channel_ptr node)
{
    if (code)
    {
        log_warning(LOG_NODE) << format(BN_CONNECTION_START_ERROR) %
            code.message();
        return;
    }

    BITCOIN_ASSERT(node);

    // Subscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&fullnode::recieve_tx, this, _1, _2, node));

    // Stay subscribed to new connections.
    protocol_.subscribe_channel(
        std::bind(&fullnode::connection_started, this, _1, _2));
}

void fullnode::recieve_tx(const std::error_code& code,
    const transaction_type& tx, channel_ptr node)
{
    if (code)
    {
        const auto hash = encode_hash(hash_transaction(tx));
        log_error(LOG_NODE) << format(BN_RECEIVE_TX_ERROR) % hash %
            code.message();
        return;
    }

    // Called when the transaction becomes confirmed in a block.
    const auto handle_confirm = [this, tx](const std::error_code& code)
    {
        const auto hash = encode_hash(hash_transaction(tx));

        if (code)
            log_error(LOG_NODE) << format(BN_CONFIRM_TX_ERROR) % hash %
                code.message();
        else
            log_debug(LOG_NODE) << format(BN_CONFIRM_TX) % hash;

        const auto handle_deindex = [](const std::error_code& code)
        {
            if (code)
                log_error(LOG_NODE) << format(BN_DEINDEX_TX_ERROR) %
                    code.message();
        };

        txidx_.deindex(tx, handle_deindex);
    };

    // Validate the transaction from the network.
    // Attempt to store in the transaction pool and check the result.
    txpool_.store(tx, handle_confirm,
        std::bind(&fullnode::new_unconfirm_valid_tx, this, _1, _2, tx));

    // Resubscribe to transaction messages from this node.
    BITCOIN_ASSERT(node);
    node->subscribe_transaction(
        std::bind(&fullnode::recieve_tx, this, _1, _2, node));
}

static std::string format_unconfirmed_inputs(const index_list& unconfirmed)
{
    if (unconfirmed.empty())
        return "";

    std::vector<std::string> inputs;
    for (const auto input: unconfirmed)
        inputs.push_back(boost::lexical_cast<std::string>(input));

    const auto list = bc::join(inputs, ",");
    const auto formatted = format(BN_WITH_UNCONFIRMED_INPUTS) % list;
    return formatted.str();
}

void fullnode::new_unconfirm_valid_tx(const std::error_code& code,
    const index_list& unconfirmed, const transaction_type& tx)
{
    auto handle_index = [](const std::error_code& code)
    {
        if (code)
            log_error(LOG_NODE) << format(BN_INDEX_ERROR) % code.message();
    };

    const auto hash = encode_hash(hash_transaction(tx));

    if (code)
        log_warning(LOG_NODE) << format(BN_MEMPOOL_ERROR) % hash %
            code.message();
    else
        log_debug(LOG_NODE) << format(BN_ACCEPTED_TRANSACTION) % hash %
            format_unconfirmed_inputs(unconfirmed);
        
    if (!code)
        txidx_.index(tx, handle_index);
}

} // namspace node
} //namespace libbitcoin