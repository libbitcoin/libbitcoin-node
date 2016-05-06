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
#include <bitcoin/node/sessions/session_block_sync.hpp>
#include <bitcoin/node/sessions/session_header_sync.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

p2p_node::p2p_node(const configuration& configuration)
  : p2p(configuration.network),
    hashes_(configuration.chain.checkpoints),
    blockchain_(configuration.chain, configuration.database),
    settings_(configuration.node)
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
    return blockchain_.pool();
}

// Start sequence.
// ----------------------------------------------------------------------------

const settings& p2p_node::node_settings() const
{
    return settings_;
}

void p2p_node::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    // The handler is invoked sequentially.
    blockchain_.start(
        std::bind(&p2p_node::handle_blockchain_start,
            shared_from_base<p2p_node>(), _1, handler));
}

void p2p_node::handle_blockchain_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::error(LOG_NODE)
            << "Blockchain failed to start: " << ec.message();
        handler(ec);
        return;
    }

    size_t height;

    if (!blockchain_.get_last_height(height))
    {
        log::error(LOG_NODE)
            << "The blockchain is not initialized with a genensis block.";
        handler(error::operation_failed);
        return;
    }

    set_height(height);

    // This is invoked on a new thread.
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

    // This is invoked on a new thread.
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
        log::error(LOG_NODE)
            << "Failure fetching blockchain start header: " << ec.message();
        handler(ec);
        return;
    }

    log::info(LOG_NODE)
        << "Blockchain height is (" << block_height << ").";

    // TODO: scan block heights to top to determine first missing block.
    // Use the block prior to the first missing block as the seed.

    // TODO: upon completion of header collection, loop over headers and
    // remove any that already exist in the chain. Sync the remainder.

    // Add the seed entry, the top trusted block hash.
    hashes_.initialize(block_header.hash(), block_height);
    const auto chain_settings = blockchain_.chain_settings();

    const auto start_handler =
        std::bind(&p2p_node::handle_headers_synchronized,
            shared_from_base<p2p_node>(), _1, handler);

    // This is invoked on a new thread.
    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_header_sync>(hashes_, settings_, chain_settings)->
        start(start_handler);
}

void p2p_node::handle_headers_synchronized(const code& ec,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure synchronizing headers: " << ec.message();
        handler(ec);
        return;
    }

    // Remove the seed entry so we don't try to sync it.
    if (!hashes_.dequeue())
    {
        log::error(LOG_NODE)
            << "Failure synchronizing headers, no seed entry.";
        handler(error::operation_failed);
        return;
    }

    const auto start_handler =
        std::bind(&p2p_node::handle_blocks_synchronized,
            shared_from_base<p2p_node>(), _1, handler);

    // This is invoked on a new thread.
    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_block_sync>(hashes_, blockchain_, settings_)->
        start(start_handler);
}

void p2p_node::handle_blocks_synchronized(const code& ec,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure synchronizing blocks: " << ec.message();
        handler(ec);
        return;
    }

    size_t chain_height;

    if (!blockchain_.get_last_height(chain_height))
    {
        log::error(LOG_NODE)
            << "The blockchain is corrupt.";
        handler(error::operation_failed);
        return;
    }

    // Node height was unchanged as there is no subscription during sync.
    set_height(chain_height);

    // TODO: skip this message if there was no sync.
    // TODO: fail if chain height is not the same as the sync target.
    // Generalize the final_height() function from protocol_header_sync.
    log::info(LOG_NODE)
        << "Blockchain height is (" << chain_height << ").";

    // This is invoked on a new thread.
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
    p2p::stop(
        std::bind(&p2p_node::handle_blockchain_stopped,
            this, _1, handler));
}

void p2p_node::handle_blockchain_stopped(const code& ec,
    result_handler handler)
{
    if (ec)
        log::error(LOG_NODE)
            << "Network shutdown error: " << ec.message();

    // This is the end of the derived stop sequence.
    blockchain_.stop(handler);
}

// Destruct sequence.
// ----------------------------------------------------------------------------

p2p_node::~p2p_node()
{
    // This allows for shutdown based on destruct without need to call stop.
    p2p_node::close();
}

void p2p_node::close()
{
    p2p_node::stop(
        std::bind(&p2p_node::handle_stopped,
            this, _1));
}

void p2p_node::handle_stopped(const code&)
{
    // This is the end of the destruct sequence.
    blockchain_.close();
}

} // namspace node
} //namespace libbitcoin
