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
#include <bitcoin/node/session.hpp>

#include <future>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

using namespace bc::blockchain;
using namespace bc::network;

session::session(threadpool& pool, handshake& handshake, protocol& protocol,
    bc::blockchain::blockchain& blockchain, poller& poller,
    transaction_pool& transaction_pool, responder& responder)
  : strand_(pool),
    handshake_(handshake),
    protocol_(protocol),
    blockchain_(blockchain),
    tx_pool_(transaction_pool),
    poller_(poller),
    responder_(responder),
    last_height_(0)
{
}

void session::start(completion_handler handle_complete)
{
    protocol_.start(
        std::bind(&session::subscribe,
            this, _1, handle_complete));
}

void session::stop(completion_handler handle_complete)
{
    protocol_.stop(handle_complete);
}

void session::subscribe(const std::error_code& ec,
    completion_handler handle_complete)
{
    if (ec)
    {
        log_error(LOG_SESSION)
            << "Failure starting session: " << ec.message();
        handle_complete(ec);
        return;
    }

    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&session::new_channel,
            this, _1, _2));

    // Subscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&session::broadcast_new_blocks,
            this, _1, _2, _3, _4));

    handle_complete(ec);
}

void session::new_channel(const std::error_code& ec, channel::pointer node)
{
    if (!node)
        return;

    const auto revive = [this, node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
        {
            log_error(LOG_SESSION)
                << "Failure in channel revival: " << ec.message();
            return;
        }

        // This should really appears as a poller log entry, since it's the
        // only actual polling.
        log_debug(LOG_SESSION)
            << "Channel revived [" << node->address().to_string() << "]";

        // Send an inv request for 500 blocks.
        poller_.request_blocks(null_hash, node);
    };

    // Revive channel with a new getblocks request if it stops getting blocks.
    node->set_revival_handler(revive);
    
    // Subscribe to new inventory requests.
    node->subscribe_inventory(
        std::bind(&session::receive_inv,
            this, _1, _2, node));

    // Subscribe to new get_blocks requests.
    node->subscribe_get_blocks(
        std::bind(&session::receive_get_blocks,
            this, _1, _2, node));

    // Resubscribe to new channels.
    protocol_.subscribe_channel(
        std::bind(&session::new_channel,
            this, _1, _2));

    // Poll this channel to build the blockchain.
    poller_.monitor(node);

    // Respond to get data requests on this channel.
    responder_.monitor(node);
}

void session::broadcast_new_blocks(const std::error_code& ec,
    uint32_t fork_point,
    const bc::blockchain::blockchain::block_list& new_blocks,
    const bc::blockchain::blockchain::block_list& /* replaced_blocks */)
{
    if (ec == bc::error::service_stopped)
        return;

    if (ec)
    {
        log_error(LOG_SESSION)
            << "Failure in reorganize: " << ec.message();
        return;
    }

    // Start height is limited to max_uint32 by satoshi protocol (version).
    BITCOIN_ASSERT((bc::max_uint32 - fork_point) >= new_blocks.size());
    const auto height = static_cast<uint32_t>(fork_point + new_blocks.size());

    const auto handle_set_height = [this, height](const std::error_code& ec)
    {
        if (ec)
        {
            log_error(LOG_SESSION)
            << "Failure setting start height: " << ec.message();
            return;
        }

        last_height_ = height;

        log_debug(LOG_SESSION)
            << "Reorg set start height [" << height << "]";
    };

    handshake_.set_start_height(height, handle_set_height);

    // Resubscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&session::broadcast_new_blocks,
            this, _1, _2, _3, _4));

    // Don't bother publishing blocks when in the initial blockchain download.
    if (fork_point < BN_CHECKPOINT_HEIGHT)
        return;

    // Broadcast new blocks inventory.
    message::inventory blocks_inventory;

    for (const auto block: new_blocks)
    {
        const message::inventory_vector inventory
        {
            message::inventory_type_id::block,
            block->header.hash()
        };

        blocks_inventory.inventories.push_back(inventory);
    }

    log_debug(LOG_SESSION)
        << "Broadcasting block inventory [" 
        << blocks_inventory.inventories.size() << "]";

    const auto broadcast_handler = [](const std::error_code& ec, size_t count)
    {
        if (ec)
            log_debug(LOG_SESSION)
                << "Failure broadcasting block inventory: " << ec.message();
        else
            log_debug(LOG_SESSION)
                << "Broadcast block inventory to (" << count << ") nodes.";
    };

    // Could optimize by not broadcasting to the node from which it came.
    protocol_.broadcast(blocks_inventory, broadcast_handler);
}

// TODO: consolidate to libbitcoin utils.
static size_t inventory_count(
    const message::inventory_vector::list& inventories,
    message::inventory_type_id type_id)
{
    size_t count = 0;

    for (const auto& inventory: inventories)
        if (inventory.type == type_id)
            ++count;

    return count;
}

// Put this on a short timer following lack of block inv.
// request_blocks(null_hash, node);
void session::receive_inv(const std::error_code& ec,
    const message::inventory& packet, channel::pointer node)
{
    if (!node)
        return;

    const auto peer = node->address().to_string();

    if (ec)
    {
        log_debug(LOG_SESSION)
            << "Failure in receive inventory ["
            << peer << "] " << ec.message();
        node->stop();
        return;
    }

    const auto blocks = inventory_count(packet.inventories,
        message::inventory_type_id::block);
    const auto transactions = inventory_count(packet.inventories,
        message::inventory_type_id::transaction);

    log_debug(LOG_SESSION)
        << "Inventory BEGIN [" << peer << "] "
        << "txs (" << transactions << ") "
        << "blocks (" << blocks << ")";

    // TODO: build an inventory vector vs. individual requests.
    // See commented out (redundant) code in poller.cpp.
    for (const auto& inventory: packet.inventories)
    {
        switch (inventory.type)
        {
            case message::inventory_type_id::transaction:
                if (last_height_ >= BN_CHECKPOINT_HEIGHT)
                {
                    log_debug(LOG_SESSION)
                        << "Transaction inventory from [" << peer << "] "
                        << encode_hash(inventory.hash);

                    strand_.queue(
                        std::bind(&session::new_tx_inventory,
                            this, inventory.hash, node));
                }

                break;

            case message::inventory_type_id::block:
                log_debug(LOG_SESSION)
                    << "Block inventory from [" << peer << "] "
                    << encode_hash(inventory.hash);
                strand_.queue(
                    std::bind(&session::new_block_inventory,
                        this, inventory.hash, node));
                break;

            case message::inventory_type_id::none:
            case message::inventory_type_id::error:
            default:
                log_debug(LOG_SESSION)
                    << "Ignoring invalid inventory type from [" << peer << "]";
        }
    }

    log_debug(LOG_SESSION)
        << "Inventory END [" << peer << "]";

    // Node may have died following new_tx_inventory or new_block_inventory.
    if (!node)
        return;

    // Resubscribe to new inventory requests.
    node->subscribe_inventory(
        std::bind(&session::receive_inv,
            this, _1, _2, node));
}

void session::new_tx_inventory(const hash_digest& tx_hash,
    channel::pointer node)
{
    if (!node)
        return;

    // If the tx doesn't exist in our mempool, issue getdata.
    tx_pool_.exists(tx_hash, 
        std::bind(&session::request_tx_data,
            this, _1, tx_hash, node));
}

void session::request_tx_data(bool tx_exists, const hash_digest& tx_hash,
    channel::pointer node)
{
    if (!node)
        return;

    if (tx_exists)
    {
        log_debug(LOG_SESSION)
            << "Transaction already exists [" << encode_hash(tx_hash) << "]";
        return;
    }

    const auto handle_error = [node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
        {
            log_debug(LOG_SESSION)
                << "Failure to get tx data from [" 
                << node->address().to_string() << "] " << ec.message();
            node->stop();
            return;
        }
    };

    log_debug(LOG_SESSION)
        << "Requesting transaction [" << encode_hash(tx_hash) << "]";

    const message::inventory_vector tx_inventory
    {
        message::inventory_type_id::transaction,
        tx_hash
    };

    message::get_data request_tx;
    request_tx.inventories.push_back(tx_inventory);
    node->send(request_tx, handle_error);
}

void session::new_block_inventory(const hash_digest& block_hash,
    channel::pointer node)
{
    if (!node)
        return;

    const auto request_block = [this, block_hash, node]
        (const std::error_code& ec, const chain::block& block)
    {
        if (ec == error::not_found)
        {
            strand_.queue(
                std::bind(&session::request_block_data,
                    this, block_hash, node));
            return;
        }

        if (ec)
        {
            log_error(LOG_SESSION)
                << "Failure fetching block ["
                << encode_hash(block_hash) << "] " << ec.message();
            node->stop();
            return;
        }

        log_debug(LOG_SESSION)
            << "Block already exists [" << encode_hash(block_hash) << "]";
    };

    // TODO: optimize with chain_.block_exists(block_hash, handler) function.
    // If the block doesn't exist, issue getdata for block.
    fetch_block(blockchain_, block_hash, request_block);
}

void session::request_block_data(const hash_digest& block_hash,
    channel::pointer node)
{
    if (!node)
        return;

    const auto handle_error = [node, block_hash](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
        {
            log_debug(LOG_SESSION)
                << "Failure to get block data from ["
                << node->address().to_string() << "] " << ec.message();
            node->stop();
        }
    };

    const message::inventory_vector block_inventory
    { 
        message::inventory_type_id::block,
        block_hash
    };

    message::get_data request_block;
    request_block.inventories.push_back(block_inventory);
    node->send(request_block, handle_error);

    // Reset the revival timer because we just asked for block inventory.
    node->reset_revival();
}

// We don't respond to peers making getblocks requests.
void session::receive_get_blocks(const std::error_code& ec,
    const message::get_blocks& get_blocks, channel::pointer node)
{
    if (!node)
        return;

    if (ec)
    {
        log_debug(LOG_SESSION)
            << "Failure in get blocks ["
            << node->address().to_string() << "] " << ec.message();
        node->stop();
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: Implement.
    // Send 500 invs from last fork point and have memory of last inv, 
    // ready to trigger send next 500 once getdata done for it.
    ///////////////////////////////////////////////////////////////////////////
    log_info(LOG_SESSION)
        << "Received a get blocks request (IGNORED).";

    // This is disabled to prevent logging subsequent requests on this channel.
    ////// Resubscribe to new get_blocks requests.
    ////node->subscribe_get_blocks(
    ////    std::bind(&session::handle_get_blocks,
    ////        this, _1, _2, node));
}

} // namespace node
} // namespace libbitcoin
