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
#include <bitcoin/node/session.hpp>

#include <future>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>

namespace libbitcoin {
namespace node {

#define NAME "session"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;

session::session(threadpool& pool, p2p& network, block_chain& blockchain,
    poller& poller, transaction_pool& transaction_pool, responder& responder,
    size_t last_checkpoint_height)
  : dispatch_(pool, NAME),
    network_(network),
    blockchain_(blockchain),
    tx_pool_(transaction_pool),
    poller_(poller),
    responder_(responder),
    last_height_(0),
    last_checkpoint_height_(last_checkpoint_height)
{
}

void session::start()
{
    // Subscribe to new connections.
    ////network_.subscribe(
    ////    std::bind(&session::new_channel,
    ////        this, _1, _2));

    // Subscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&session::handle_new_blocks,
            this, _1, _2, _3, _4));
}

void session::new_channel(const code& ec, channel::ptr node)
{
    // This is the sentinel code for protocol stopping (and node is nullptr).
    if (ec == error::service_stopped)
        return;

    const auto revive = [this, node](const code ec)
    {
        if (ec)
        {
            log::error(LOG_SESSION)
                << "Failure in channel revival: " << ec.message();
            return;
        }

        log::debug(LOG_SESSION)
            << "Channel revived [" << node->authority() << "]";

        // Send an inv request for 500 blocks.
        poller_.request_blocks(null_hash, node);
    };

    // Revive channel with a new getblocks request if it stops getting blocks.
    node->set_revival_handler(revive);
    
    ////////// Subscribe to new inventory requests.
    ////////node->subscribe<inventory>(
    ////////    std::bind(&session::receive_inv,
    ////////        this, _1, _2, node));

    ////////// Subscribe to new get_blocks requests.
    ////////node->subscribe<get_blocks>(
    ////////    std::bind(&session::receive_get_blocks,
    ////////        this, _1, _2, node));

    ////////// Resubscribe to new channels.
    ////////network_.subscribe(
    ////////    std::bind(&session::new_channel,
    ////////        this, _1, _2));

    // Poll this channel to build the blockchain.
    poller_.monitor(node);

    // Respond to get data requests on this channel.
    responder_.monitor(node);
}

void session::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block_chain::list& new_blocks,
    const block_chain::list& /* replaced_blocks */)
{
    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        log::error(LOG_SESSION)
            << "Failure in reorganize: " << ec.message();
        return;
    }

    // Start height is limited to max_uint32 by satoshi protocol (version).
    BITCOIN_ASSERT((bc::max_uint32 - fork_point) >= new_blocks.size());
    const auto height = static_cast<uint32_t>(fork_point + new_blocks.size());

    network_.set_height(height);

    log::debug(LOG_SESSION)
        << "Reorganize set start height [" << height << "]";

    // Resubscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&session::handle_new_blocks,
            this, _1, _2, _3, _4));

    // Don't bother publishing blocks when in the initial blockchain download.
    if (fork_point < last_checkpoint_height_)
        return;

    // Broadcast new blocks inventory.
    inventory blocks_inventory;

    for (const auto block: new_blocks)
    {
        const inventory_vector inventory
        {
            inventory_type_id::block,
            block->header.hash()
        };

        blocks_inventory.inventories.push_back(inventory);
    }

    log::debug(LOG_SESSION)
        << "Broadcasting block inventory [" 
        << blocks_inventory.inventories.size() << "]";

    const auto broadcast_handler = [](const code ec, channel::ptr node)
    {
        if (ec)
            log::debug(LOG_SESSION)
                << "Failure broadcasting block inventory to ["
                << node->authority() << "] " << ec.message();
        else
            log::debug(LOG_SESSION)
                << "Broadcasted block inventory to ["
                << node->authority() << "] ";
    };

    // Could optimize by not broadcasting to the node from which it came.
    const auto unhandled = [](const code){};
    network_.broadcast(blocks_inventory, broadcast_handler, unhandled);
}

// Put this on a short timer following lack of block inv.
// request_blocks(null_hash, node);
void session::receive_inv(const code& ec, const inventory& packet,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    const auto peer = node->authority();

    if (ec)
    {
        log::debug(LOG_SESSION)
            << "Failure in receive inventory ["
            << peer << "] " << ec.message();
        node->stop(ec);
        return;
    }

    ////////// Resubscribe to new inventory requests.
    ////////node->subscribe<inventory>(
    ////////    std::bind(&session::receive_inv,
    ////////        this, _1, _2, node));

    log::debug(LOG_RESPONDER)
        << "Inventory BEGIN [" << peer << "] "
        << "txs (" << packet.count(inventory_type_id::transaction) << ") "
        << "blocks (" << packet.count(inventory_type_id::block) << ") "
        << "bloom (" << packet.count(inventory_type_id::filtered_block) << ")";

    // TODO: build an inventory vector vs. individual requests.
    // See commented out (redundant) code in poller.cpp.
    for (const auto& inventory: packet.inventories)
    {
        switch (inventory.type)
        {
            case inventory_type_id::transaction:
                if (last_height_ >= last_checkpoint_height_)
                {
                    log::debug(LOG_SESSION)
                        << "Transaction inventory from [" << peer << "] "
                        << encode_hash(inventory.hash);

                    dispatch_.ordered(
                        std::bind(&session::new_tx_inventory,
                            this, inventory.hash, node));
                }
                else
                    log::debug(LOG_SESSION)
                        << "Ignoring premature transaction inventory from ["
                        << peer << "]";

                break;

            case inventory_type_id::block:
                log::debug(LOG_SESSION)
                    << "Block inventory from [" << peer << "] for ["
                    << encode_hash(inventory.hash) << "]";
                dispatch_.ordered(
                    std::bind(&session::new_block_inventory,
                        this, inventory.hash, node));
                break;

            case inventory_type_id::filtered_block:
                // We don't suppport bloom filters, so we shouldn't see this.
                log::debug(LOG_SESSION)
                    << "Ignoring filtered block inventory from ["
                    << peer << "] " << encode_hash(inventory.hash);
                break;

            case inventory_type_id::none:
            case inventory_type_id::error:
            default:
                log::debug(LOG_SESSION)
                    << "Ignoring invalid inventory type from [" << peer << "]";
        }
    }

    log::debug(LOG_SESSION)
        << "Inventory END [" << peer << "]";
}

void session::new_tx_inventory(const hash_digest& hash, channel::ptr node)
{
    // If the tx doesn't exist in our mempool, issue getdata.
    tx_pool_.exists(hash,
        std::bind(&session::request_tx_data,
            this, _1, hash, node));
}

void session::request_tx_data(const code& ec, const hash_digest& hash,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    const auto encoded = encode_hash(hash);

    if (ec == error::success)
    {
        log::debug(LOG_SESSION)
            << "Transaction already exists [" << encoded << "]";
        return;
    }

    if (ec != error::not_found)
    {
        log::debug(LOG_SESSION)
            << "Failure in getting transaction existence [" << encoded << "] "
            << ec.message();
        return;
    }

    // ec == error::not_found

    const auto handle_error = [node, hash](const code ec)
    {
        if (ec)
        {
            log::debug(LOG_SESSION)
                << "Failure sending tx [" << encode_hash(hash)
                << "] request to [" << node->authority() << "] "
                << ec.message();
            node->stop(ec);
            return;
        }
    };

    log::debug(LOG_SESSION)
        << "Requesting transaction [" << encoded << "]";

    const get_data packet{ { inventory_type_id::transaction, hash } };
    node->send(packet, handle_error);
}

void session::new_block_inventory(const hash_digest& hash, channel::ptr node)
{
    const auto request_block = [this, hash, node]
        (const code ec, const block block)
    {
        if (ec == error::not_found)
        {
            dispatch_.ordered(
                std::bind(&session::request_block_data,
                    this, hash, node));
            return;
        }

        if (ec)
        {
            log::error(LOG_SESSION)
                << "Failure fetching block ["
                << encode_hash(hash) << "] " << ec.message();
            node->stop(ec);
            return;
        }

        log::debug(LOG_SESSION)
            << "Block already exists [" << encode_hash(hash) << "]";
    };

    // TODO: optimize with chain_.block_exists(block_hash, handler) function.
    // If the block doesn't exist, issue getdata for block.
    block_fetcher::fetch(blockchain_, hash, request_block);
}

void session::request_block_data(const hash_digest& hash, channel::ptr node)
{
    log::debug(LOG_SESSION)
        << "Requesting block [" << encode_hash(hash) << "] from ["
        << node->authority() << "]";

    const auto handle_error = [node, hash](const code ec)
    {
        if (ec)
        {
            log::debug(LOG_SESSION)
                << "Failure requesting block data [" << encode_hash(hash)
                << "] from [" << node->authority() << "] " << ec.message();
            node->stop(ec);
            return;
        }

        log::debug(LOG_SESSION)
            << "Sent block request [" << encode_hash(hash) << "] to ["
            << node->authority() << "]";
    };

    const get_data packet{ { inventory_type_id::block, hash } };
    node->send(packet, handle_error);

    // Reset the revival timer because we just asked for block data. If after
    // the last revival-initiated inventory request we didn't receive any block
    // inv then this will not restart the timer and we will no longer revive
    // this channel.
    //
    // The presumption is that we are then at the top of our peer's chain, or
    // the peer has delayed but will eventually send us more block inventory, 
    // thereby restarting the revival timer.
    //
    // If we have not sent a block inv request because the current inv request
    // is the same as the last then this may stall. So we skip a duplicate
    // request only if the last request was not a null_hash stop (500).
    //
    // If the peer is just unresponsive but we are not at its top, we will end
    // up timing out or expiring the channel.
    node->reset_revival();
}

// TODO: We don't currently respond to peers making getblocks requests.
void session::receive_get_blocks(const code& ec, const get_blocks& get_blocks,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec)
    {
        log::debug(LOG_SESSION)
            << "Failure in get blocks [" << node->authority() << "] "
            << ec.message();
        node->stop(ec);
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: Implement.
    ///////////////////////////////////////////////////////////////////////////
    log::info(LOG_SESSION)
        << "Received a get blocks request (IGNORED).";

    // This is disabled to prevent logging subsequent requests on this channel.
    ////// Resubscribe to new get_blocks requests.
    ////node->subscribe_get_blocks(
    ////    std::bind(&session::handle_get_blocks,
    ////        this, _1, _2, node));
}

} // namespace node
} // namespace libbitcoin
