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

#include <cstddef>
#include <memory>
#include <system_error>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

/// Request block inventory, receive and store blocks.
poller::poller(blockchain& chain, size_t minimum_start_height)
  : blockchain_(chain),
    last_height_(0),
    minimum_start_height_(minimum_start_height)
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

    // Poll channel with a new getblocks request after stop getting blocks.
    node->set_revival_handler(
        std::bind(&poller::handle_revive,
            this, _1, node));

    // Subscribe to reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&poller::handle_reorg,
            this, _1, _2, _3, _4));

    // Make initial block inventory request.
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

    // Move the block to the blockchain queue.
    // This may keep the channel instance alive for a long time after stop.
    blockchain_.store(std::make_shared<block_type>(block),
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

            // We suspend this during sync because it isn't necessary and block
            // announcements and loss of order can cause excessive backlogging.
            if (last_height_ < minimum_start_height_)
                return;

            // This is how we get missing blocks between top and the orphan.
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
    fetch_block_locator(blockchain_,
        std::bind(&poller::ask_blocks,
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
}

// Handle reorganization (set local height)
// ----------------------------------------------------------------------------

bool poller::handle_reorg(const std::error_code& ec, uint32_t fork_point,
    const blockchain::block_list& new_blocks, const blockchain::block_list&)
{
    if (ec == error::service_stopped)
        return false;

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure handling reorg: " << ec.message();
        return false;
    }

    // Start height is limited to max_uint32 by satoshi protocol (version).
    BITCOIN_ASSERT((bc::max_uint32 - fork_point) >= new_blocks.size());
    const auto height = static_cast<uint32_t>(fork_point + new_blocks.size());

    // atomic
    last_height_ = height;
    return true;
}


} // namespace node
} // namespace libbitcoin
