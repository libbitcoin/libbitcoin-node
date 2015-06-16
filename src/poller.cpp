/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::io_service;

poller::poller(threadpool& pool, blockchain::blockchain& chain)
  : strand_(pool),
    blockchain_(chain),
    last_block_hash_(null_hash),
    last_locator_begin_(null_hash),
    last_hash_stop_(null_hash),
    last_requested_node_(nullptr)
{
}

// Start monitoring this channel.
void poller::monitor(channel_ptr node)
{
    if (!node)
    {
        log_error(LOG_POLLER)
            << "The node is not initialized and cannot be monitored.";
        return;
    }

    //////node->subscribe_inventory(
    //////    std::bind(&poller::receive_inv,
    //////        this, _1, _2, node));

    node->subscribe_block(
        std::bind(&poller::receive_block,
            this, _1, _2, node));

    // Issue the initial ask for blocks.
    request_blocks(null_hash, node);
}

// Dead code, temp retain for example.
//void poller::receive_inv(const std::error_code& ec,
//    const inventory_type& packet, channel_ptr node)
//{
//    if (!node)
//        return;
//
//    const auto peer = node->address().to_string();
//
//    if (ec)
//    {
//        log_debug(LOG_POLLER)
//            << "Failure in receive inventory ["
//            << peer << "] " << ec.message();
//        node->stop();
//        return;
//    }
//
//    log_debug(LOG_POLLER)
//        << "Inventory BEGIN [" << peer << "] ("
//        << packet.inventories.size() << ")";
//
//    get_data_type getdata;
//    for (const auto& inventory: packet.inventories)
//    {
//        // Filter out non-block inventories
//        if (inventory.type == inventory_type_id::block)
//        {
//            log_debug(LOG_POLLER)
//                << "Block inventory from [" << peer << "] "
//                << encode_hash(inventory.hash);
//
//            // Exclude the last block asked for.
//            // Presumably this is because it's convenient/fast to do.
//            // TODO: filter out blocks we already have so we aren't
//            // re-requesting block data for them. Presumably we could
//            // have obtained them on another channel even though we 
//            // did ask for them on this channel.
//            if (inventory.hash != last_block_hash_)
//                getdata.inventories.push_back(inventory);
//        }
//    }
//
//    log_debug(LOG_POLLER)
//        << "Inventory END [" << peer << "]";
//
//    // The session is also subscribed to inv, so we don't handle tx here.
//    if (getdata.inventories.empty())
//    {
//        // If empty there is no ask, resulting in a stall.
//        log_debug(LOG_POLLER) 
//            << "Received empty filtered block inventory.";
//    }
//    else
//    {
//        last_block_hash_ = getdata.inventories.back().hash;
//        node->send(getdata, handle_send_packet);
//    }
//
//    // Resubscribe.
//    node->subscribe_inventory(
//        strand_.wrap(&poller::receive_inv,
//            this, _1, _2, node));
//}

void poller::receive_block(const std::error_code& ec,
    const chain::block& block, channel_ptr node)
{
    if (!node)
        return;

    if (ec)
    {
        log_warning(LOG_POLLER) << "Received bad block: "
            << ec.message();
        node->stop(/* ec */);
        return;
    }

    blockchain_.store(block,
        strand_.wrap(&poller::handle_store_block,
            this, _1, _2, block.header.hash(), node));

    // Resubscribe.
    node->subscribe_block(
        std::bind(&poller::receive_block,
            this, _1, _2, node));
}

void poller::handle_store_block(const std::error_code& ec,
    blockchain::block_info info, const hash_digest& block_hash,
    channel_ptr node)
{
    if (ec == error::duplicate)
    {
        // This is common, we get blocks we already have.
        log_debug(LOG_POLLER) << "Redundant block ["
            << encode_hash(block_hash) << "]";
        return;
    }

    if (ec)
    {
        log_error(LOG_POLLER) << "Error storing block ["
            << encode_hash(block_hash) << "] " << ec.message();
        node->stop(/* ec */);
        return;
    }
    
    switch (info.status)
    {
        // The block has been accepted as an orphan (ec not set).
        case blockchain::block_status::orphan:
            log_debug(LOG_POLLER) << "Potential block ["
                << encode_hash(block_hash) << "]";

            // This is how we get other nodes to send us the blocks we are
            // missing from the top of our chain to the orphan.
            request_blocks(block_hash, node);
            break;

        // The block has been rejected from the store (redundant?).
        // This case may be redundant with error::duplicate.
        case blockchain::block_status::rejected:
            log_debug(LOG_POLLER) << "Rejected block ["
                << encode_hash(block_hash) << "]";
            break;

        // The block has been accepted into the long chain (ec not set).
        case blockchain::block_status::confirmed:
            log_info(LOG_POLLER) << "Block #"
                << info.height << " " << encode_hash(block_hash);
            break;
    }
}

void poller::request_blocks(const hash_digest& block_hash, channel_ptr node)
{
    fetch_block_locator(blockchain_,
        strand_.wrap(&poller::ask_blocks,
            this, _1, _2, block_hash, node));
}

// Not having orphans will cause a stall 
void poller::ask_blocks(const std::error_code& ec,
    const message::block_locator& locator, const hash_digest& hash_stop,
    channel_ptr node)
{
    if (!node)
        return;

    if (ec)
    {
        log_debug(LOG_POLLER)
            << "Failed to fetch block locator: " << ec.message();
        return;
    }
    
    if (is_duplicate_block_ask(locator, hash_stop, node))
    {
        log_debug(LOG_POLLER)
            << "Skipping duplicate ask blocks with locator front ["
            << encode_hash(locator.front()) << "]";
        return;
    }

    log_debug(LOG_POLLER)
        << "Ask for blocks with stop [" 
        << encode_hash(hash_stop) << "]";

    const auto handle_error = [node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
        {
            log_debug(LOG_POLLER)
                << "Send get blocks problem: " << ec.message();

            // TODO: modify send() to terminate the connection on send failure.
            node->stop();
        }
    };

    // Send get_blocks request.
    const message::get_blocks packet;
    packet.start_hashes = locator;
    packet.hash_stop = hash_stop;
    node->send(packet, handle_error);

    // Update last values.
    last_locator_begin_ = locator.front();
    last_hash_stop_ = hash_stop;
    last_requested_node_ = node.get();
}

bool poller::is_duplicate_block_ask(const block_locator_type& locator,
    const hash_digest& hash_stop, channel_ptr node)
{
    return
        last_locator_begin_ == locator.front() &&
        last_hash_stop_ == hash_stop &&
        last_requested_node_ == node.get();
}

} // namespace node
} // namespace libbitcoin
