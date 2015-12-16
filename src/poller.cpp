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
#include <bitcoin/node/poller.hpp>

#include <system_error>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::io_service;

/// Request block inventory, receive and store blocks.
poller::poller(threadpool& pool, blockchain& chain)
  : strand_(pool),
    blockchain_(chain),
    last_locator_begin_(null_hash),
    last_hash_stop_(null_hash),
    last_block_ask_node_(nullptr)
{
}

// Startup
// ----------------------------------------------------------------------------

// Start monitoring this channel.
void poller::monitor(channel_ptr node)
{
    // Subscribe to block messages.
    node->subscribe_block(
        std::bind(&poller::receive_block,
            this, _1, _2, node));

    // Revive channel with a new getblocks request if it stops getting blocks.
    node->set_revival_handler(
        std::bind(&poller::handle_revive,
            this, _1, node));

    // TODO: consider deferring this ask on inbound connections.
    // The caller may intend only to post a transaction and disconnect.

    // Issue the initial ask for blocks.
    handle_revive(error::success, node);
}

// Handle block receipt timeout (revivial)
// ----------------------------------------------------------------------------

void poller::handle_revive(const std::error_code& ec, channel_ptr node)
{
    if (ec)
    {
        log_error(LOG_SESSION)
            << "Failure in initial block request: " << ec.message();
        return;
    }

    // Send an inv request for 500 blocks.
    request_blocks(null_hash, node);
}

// Handle block message
// ----------------------------------------------------------------------------

bool poller::receive_block(const std::error_code& ec, const block_type& block,
    channel_ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    if (ec)
    {
        log_warning(LOG_POLLER)
            << "Received bad block: " << ec.message();
        node->stop(ec);
        return false;
    }

    blockchain_.store(block,
        std::bind(&poller::handle_store_block,
            this, _1, _2, hash_block_header(block.header), node));

    // Reset the revival timer because we just recieved a block from this peer.
    // Once we are at the top this will end up polling the peer.
    node->reset_revival();
    return true;
}

void poller::handle_store_block(const std::error_code& ec, block_info info,
    const hash_digest& block_hash, channel_ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::duplicate)
    {
        // This is common, we get blocks we already have.
        log_debug(LOG_POLLER)
            << "Redundant block [" << encode_hash(block_hash) << "]";
        return;
    }

    if (ec)
    {
        log_error(LOG_POLLER)
            << "Error storing block [" << encode_hash(block_hash) << "] "
            << ec.message();
        node->stop(ec);
        return;
    }
    
    switch (info.status)
    {
        // The block has been accepted as an orphan (ec not set).
        case block_status::orphan:
            log_debug(LOG_POLLER)
                << "Potential block [" << encode_hash(block_hash) << "]";

            // This is how we get other nodes to send us the blocks we are
            // missing from the top of our chain to the orphan.
            request_blocks(block_hash, node);
            break;

        // The block has been rejected from the store (redundant?).
        // This case may be redundant with error::duplicate.
        case block_status::rejected:
            log_debug(LOG_POLLER)
                << "Rejected block [" << encode_hash(block_hash) << "]";
            break;

        // This may have also caused blocks to be accepted via the pool.
        // The block has been accepted into the long chain (ec not set).
        case block_status::confirmed:
            log_info(LOG_POLLER)
                << "Block #" << info.height << " " << encode_hash(block_hash);
            break;
    }
}

// Request blocks (500 at startup and revivial, fill gap otherwise)
// ----------------------------------------------------------------------------

void poller::request_blocks(const hash_digest& block_hash, channel_ptr node)
{
    // strand guards last_ members.
    fetch_block_locator(blockchain_,
        strand_.wrap(&poller::ask_blocks,
            this, _1, _2, block_hash, node));
}

void poller::ask_blocks(const std::error_code& ec,
    const block_locator_type& locator, const hash_digest& hash_stop,
    channel_ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        log_debug(LOG_POLLER)
            << "Failed to fetch block locator: " << ec.message();
        return;
    }

    BITCOIN_ASSERT(!locator.empty());
    
    if (is_duplicate_block_ask(locator, hash_stop, node))
    {
        log_debug(LOG_POLLER)
            << "Skipping duplicate ask blocks with locator front ["
            << encode_hash(locator.front()) << "]";
        return;
    }
    
    const auto stop = hash_stop == null_hash ? "500" : encode_hash(hash_stop);
    log_debug(LOG_POLLER)
        << "Ask for blocks from [" << encode_hash(locator.front()) << "]("
        << locator.size() << ") to [" << stop << "]";

    const auto handle_error = [node](std::error_code ec)
    {
        if (ec)
        {
            log_debug(LOG_POLLER)
                << "Failure sending get blocks: " << ec.message();
            node->stop(ec);
        }
    };

    // Send get_blocks request.
    const get_blocks_type packet{ locator, hash_stop };
    node->send(packet, handle_error);

    // guarded
    last_locator_begin_ = locator.front();
    last_hash_stop_ = hash_stop;
    last_block_ask_node_ = node.get();
}

bool poller::is_duplicate_block_ask(const block_locator_type& locator,
    const hash_digest& hash_stop, channel_ptr node)
{
    // guarded by ask_blocks
    return
        last_locator_begin_ == locator.front() &&
        last_hash_stop_ == hash_stop && hash_stop != null_hash &&
        last_block_ask_node_ == node.get();
}

} // namespace node
} // namespace libbitcoin
