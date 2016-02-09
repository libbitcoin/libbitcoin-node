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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/session_block_sync.hpp>
#include <bitcoin/node/session_header_sync.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

///////////////////////////////////////////////////////////////////////////////
// TODO: move blockchain threadpool into blockchain and transaction pool start.
// This requires that the blockchain components support start, stop and close.
///////////////////////////////////////////////////////////////////////////////

p2p_node::p2p_node(const configuration& configuration)
  : p2p(configuration.network),
    configuration_(configuration),
    blockchain_threadpool_(0),
    blockchain_(blockchain_threadpool_, configuration.chain),
    transaction_pool_(blockchain_threadpool_, blockchain_, configuration.chain)
{
}

// Properties.
// ----------------------------------------------------------------------------

block_chain& p2p_node::chain()
{
    return blockchain_;
}

transaction_pool& p2p_node::pool()
{
    return transaction_pool_;
}

// Start sequence.
// ----------------------------------------------------------------------------

const configuration& p2p_node::configured() const
{
    return configuration_;
}

void p2p_node::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    blockchain_threadpool_.join();
    blockchain_threadpool_.spawn(configuration_.chain.threads,
        thread_priority::low);

    blockchain_.start(
        std::bind(&p2p_node::handle_blockchain_start,
            shared_from_base<p2p_node>(), _1, handler));
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
            shared_from_base<p2p_node>(), _1, _2, handler));
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

    // TODO: iron this out.
    BITCOIN_ASSERT(height <= bc::max_size_t);
    const auto safe_height = static_cast<size_t>(height);
    set_height(safe_height);

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

    // Ensure consistency in the case where member height is changing.
    const auto current_height = height();

    blockchain_.fetch_block_header(current_height,
        std::bind(&p2p_node::handle_fetch_header,
            shared_from_base<p2p_node>(), _1, _2, current_height, handler));
}

void p2p_node::handle_fetch_header(const code& ec, const header& block_header,
    size_t block_height, result_handler handler)
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

    const config::checkpoint top(block_header.hash(), block_height);

    const auto start_handler =
        std::bind(&p2p_node::handle_headers_synchronized,
            shared_from_base<p2p_node>(), _1, top.height(), handler);

    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_header_sync>(hashes_, top, configuration_)->start(
        start_handler);
}

void p2p_node::handle_headers_synchronized(const code& ec, size_t block_height,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Failure synchronizing headers: " << ec.message();
        handler(ec);
        return;
    }

    if (hashes_.empty())
    {
        log::info(LOG_NETWORK)
            << "Completed header synchronization.";
        handle_blocks_synchronized(error::success, block_height, handler);
        return;
    }

    // First height in hew headers.
    const auto first_height = block_height + 1;
    const auto end_height = first_height + hashes_.size() - 1;

    log::info(LOG_NETWORK)
        << "Completed header synchronization [" << first_height << "-"
        << end_height << "]";

    const auto start_handler =
        std::bind(&p2p_node::handle_blocks_synchronized,
            shared_from_base<p2p_node>(), _1, first_height, handler);

    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_block_sync>(hashes_, first_height, configuration_,
        blockchain_)->start(start_handler);
}

void p2p_node::handle_blocks_synchronized(const code& ec, size_t start_height,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Failure synchronizing blocks: " << ec.message();
        handler(ec);
        return;
    }

    log::info(LOG_NETWORK)
        << "Completed block synchronization [" << start_height
        << "-" << height() << "]";

    // This is the end of the derived run sequence.
    p2p::run(handler);
}

// Subscribers.
// ----------------------------------------------------------------------------

void p2p_node::subscribe_blockchain(reorganize_handler handler)
{
    chain().subscribe_reorganize(handler);

}

void p2p_node::subscribe_transaction_pool(transaction_handler handler)
{
    pool().subscribe_transaction(handler);
}

// Stop sequence.
// ----------------------------------------------------------------------------

void p2p_node::stop(result_handler handler)
{
    blockchain_.stop();
    transaction_pool_.stop();
    blockchain_threadpool_.shutdown();

    // This is the end of the derived stop sequence.
    p2p::stop(handler);
}

// Destruct sequence.
// ----------------------------------------------------------------------------

void p2p_node::close()
{
    p2p_node::stop(unhandled);

    blockchain_threadpool_.join();

    // This is the end of the destruct sequence.
    p2p::close();
}

p2p_node::~p2p_node()
{
    // A reference cycle cannot exist with this class, since we don't capture
    // shared pointers to it. Therefore this will always clear subscriptions.
    // This allows for shutdown based on destruct without need to call stop.
    p2p_node::close();
}

} // namspace node
} //namespace libbitcoin
