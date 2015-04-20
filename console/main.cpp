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
#include <bitcoin/node.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::node;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

void log_to_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    file << level_repr(level);
    if (!domain.empty())
        file << " [" << domain << "]";
    file << ": " << body << std::endl;
}
void log_to_both(std::ostream& device, std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    std::ostringstream output;
    const std::time_t unix_time = std::time(nullptr);
    output << unix_time << " " << level_repr(level);
    if (!domain.empty())
        output << " [" << domain << "]";
    output << ": " << body;
    device << output.str() << std::endl;
    file << output.str() << std::endl;
}

void output_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_file(file, level, domain, body);
}
void output_both(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_both(std::cout, file, level, domain, body);
}

void error_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_file(file, level, domain, body);
}
void error_both(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_both(std::cerr, file, level, domain, body);
}

class fullnode
{
public:
    fullnode(const std::string& db_prefix);
    void start();
    // Should only be called from the main thread.
    // It's an error to join() a thread from inside it.
    void stop();

    blockchain& chain();
    transaction_indexer& indexer();

private:
    void handle_start(const std::error_code& ec);

    // New connection has been started.
    // Subscribe to new transaction messages from the network.
    void connection_started(const std::error_code& ec,
        network::channel_ptr node);
    // New transaction message from the network.
    // Attempt to validate it by storing it in the transaction pool.
    void recv_tx(const std::error_code& ec,
        const transaction_type& tx, network::channel_ptr node);
    // Result of store operation in transaction pool.
    void new_unconfirm_valid_tx(
        const std::error_code& ec, const index_list& unconfirmed,
        const transaction_type& tx);

    libbitcoin::threadpool net_pool_;
    libbitcoin::threadpool disk_pool_;
    libbitcoin::threadpool mem_pool_;
    network::hosts hosts_;
    network::handshake handshake_;
    network::network network_;
    network::protocol protocol_;
    chain::blockchain_impl chain_;
    node::poller poller_;
    chain::transaction_pool txpool_;
    node::transaction_indexer txidx_;
    node::session session_;
};

fullnode::fullnode(const std::string& db_prefix)
    // Threadpools and the number of threads they spawn.
    // 6 threads spawned in total.
  : net_pool_(1), 
    disk_pool_(4), 
    mem_pool_(1),
    // Networking related services.
    hosts_(net_pool_), 
    handshake_(net_pool_), 
    network_(net_pool_),
    protocol_(net_pool_, hosts_, handshake_, network_),
    // Blockchain database service.
    chain_(disk_pool_, db_prefix, {0}),
    // Poll new blocks, and transaction memory pool.
    poller_(mem_pool_, chain_), 
    txpool_(mem_pool_, chain_), 
    txidx_(mem_pool_),
    // Session manager service. Convenience wrapper.
    session_(net_pool_, {
        handshake_, protocol_, chain_, poller_, txpool_})
{
}

void fullnode::start()
{
    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&fullnode::connection_started, this, _1, _2));
    // Start blockchain
    DEBUG_ONLY(bool chain_started =) chain_.start();
    BITCOIN_ASSERT(chain_started);
    // Start transaction pool
    txpool_.start();
    // Fire off app.
    auto handle_start =
        std::bind(&fullnode::handle_start, this, _1);
    session_.start(handle_start);
}

void fullnode::stop()
{
    std::promise<std::error_code> ec_promise;
    auto session_stopped = [&ec_promise](const std::error_code& ec)
    {
        ec_promise.set_value(ec);
    };

    session_.stop(session_stopped);
    std::error_code ec = ec_promise.get_future().get();
    if (ec)
        log_error() << "Problem stopping session: " << ec.message();

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

blockchain& fullnode::chain()
{
    return chain_;
}
transaction_indexer& fullnode::indexer()
{
    return txidx_;
}

void fullnode::handle_start(const std::error_code& ec)
{
    if (ec)
        log_error() << "fullnode: " << ec.message();
}

void fullnode::connection_started(const std::error_code& ec,
    network::channel_ptr node)
{
    if (ec)
    {
        log_warning() << "Couldn't start connection: " << ec.message();
        return;
    }

    // Subscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&fullnode::recv_tx, this, _1, _2, node));

    // Stay subscribed to new connections.
    protocol_.subscribe_channel(
        std::bind(&fullnode::connection_started, this, _1, _2));
}

void fullnode::recv_tx(const std::error_code& ec,
    const transaction_type& tx, network::channel_ptr node)
{
    if (ec)
    {
        log_error() << "Receive transaction: " << ec.message();
        return;
    }

    auto handle_deindex = [](const std::error_code& ec)
    {
        if (ec)
            log_error() << "Deindex error: " << ec.message();
    };

    // Called when the transaction becomes confirmed in a block.
    auto handle_confirm = [this, tx, handle_deindex](const std::error_code& ec)
    {
        const auto& encoded_tx_hash = encode_hash(hash_transaction(tx));

        log_debug() << "handle_confirm ec = " << ec.message() << " " 
            << encoded_tx_hash;
        if (ec)
            log_error() << "Confirm error (" << encoded_tx_hash << "): " 
            << ec.message();
        txidx_.deindex(tx, handle_deindex);
    };

    // Validate the transaction from the network.
    // Attempt to store in the transaction pool and check the result.
    txpool_.store(tx, handle_confirm,
        std::bind(&fullnode::new_unconfirm_valid_tx, this, _1, _2, tx));

    // Resubscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&fullnode::recv_tx, this, _1, _2, node));
}

void fullnode::new_unconfirm_valid_tx(
    const std::error_code& ec, const index_list& unconfirmed,
    const transaction_type& tx)
{
    auto handle_index = [](const std::error_code& ec)
    {
        if (ec)
            log_error() << "Index error: " << ec.message();
    };

    const auto& encoded_tx_hash = encode_hash(hash_transaction(tx));

    if (ec)
    {
        log_warning()
            << "Error storing memory pool transaction "
            << encoded_tx_hash << ": " << ec.message();
    }
    else
    {
        auto log = log_debug();
        log << "Accepted transaction ";

        if (!unconfirmed.empty())
        {
            log << "(Unconfirmed inputs";
            for (auto idx: unconfirmed)
                log << " " << idx;
            log << ") ";
        }

        log << encoded_tx_hash;
        txidx_.index(tx, handle_index);
    }
}

void history_fetched(const std::error_code& ec, const history_list& history)
{
    if (ec)
    {
        log_error() << "Failed to fetch history: " << ec.message();
        return;
    }

    log_info() << "Query fine.";

    for (const auto& row: history)
    {
        if (row.id == point_ident::output)
            std::cout << "OUTPUT: ";
        else //if (row.id == point_ident::spend)
            std::cout << "SPEND:  ";
        std::cout << encode_hash(row.point.hash) << ":" << row.point.index
            << " " << row.height << " " << row.value << std::endl;
    }
}

//This Expects the blockchain to be present in "./blockchain/" and initialized
//using initchain (from libbitcoin-blockchain/tools/)
int main()
{
    bc::ofstream debug_log_file("debug.log");
    log_debug().set_output_function(
        std::bind(output_file, std::ref(debug_log_file), _1, _2, _3));
    log_info().set_output_function(
        std::bind(output_both, std::ref(debug_log_file), _1, _2, _3));

    bc::ofstream error_log_file("error.log");
    log_warning().set_output_function(
        std::bind(error_file, std::ref(error_log_file), _1, _2, _3));
    log_error().set_output_function(
        std::bind(error_both, std::ref(error_log_file), _1, _2, _3));
    log_fatal().set_output_function(
        std::bind(error_both, std::ref(error_log_file), _1, _2, _3));

    fullnode app("blockchain");
    app.start();

    while (true)
    {
        std::string addr;
        std::getline(std::cin, addr);
        if (addr == "stop")
            break;

        payment_address payaddr;
        if (!payaddr.set_encoded(addr))
        {
            log_error() << "Skipping invalid Bitcoin address.";
            continue;
        }

        fetch_history(app.chain(), app.indexer(), payaddr, history_fetched);
    }

    log_info() << "Shutting down...";
    app.stop();

    return 0;
}