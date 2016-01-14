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

#include <memory>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

#define NAME "poller"

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

poller::poller(threadpool& pool, block_chain& chain)
  : dispatch_(pool, NAME),
    blockchain_(chain)
{
}

// Start monitoring this channel.
void poller::monitor(channel::ptr node)
{
    ////node->subscribe<message::inventory>(
    ////    std::bind(&poller::receive_inv,
    ////        this, _1, _2, node));

    ////node->subscribe<block>(
    ////    std::bind(&poller::receive_block,
    ////        this, _1, _2, node));

    request_blocks(null_hash, node);
}

void poller::receive_block(const code& ec, const block& block,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec)
    {
        log::warning(LOG_POLLER)
            << "Received bad block from [" << node->authority() << "] "
            << ec.message();
        node->stop(ec);
        return;
    }

    //////node->subscribe<message::block>(
    //////    std::bind(&poller::receive_block,
    //////        this, _1, _2, node));

    const auto block_ptr = std::make_shared<chain::block>(block);

    blockchain_.store(block_ptr,
        std::bind(&poller::handle_store_block,
            this, _1, _2, block.header.hash(), node));
}

void poller::handle_store_block(const code& ec, const block_info& info,
    const hash_digest& hash, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    const auto encoded = encode_hash(hash);

    if (ec == error::duplicate)
    {
        // This is common, we get blocks we already have.
        log::debug(LOG_POLLER)
            << "Redundant block [" << encoded << "]";
        return;
    }

    if (ec)
    {
        log::error(LOG_POLLER)
            << "Error storing block [" << encoded << "] from ["
            << node->authority() << "] " << ec.message();
        node->stop(ec);
        return;
    }
    
    switch (info.status)
    {
        // The block has been accepted as an orphan (ec not set).
        case block_status::orphan:
            log::debug(LOG_POLLER)
                << "Potential block [" << encoded << "]";

            // This is how we get other nodes to send us the blocks we are
            // missing from the top of our chain to the orphan.
            request_blocks(hash, node);
            break;

        // The block has been rejected from the store (redundant?).
        // This case may be redundant with error::duplicate.
        case block_status::rejected:
            log::debug(LOG_POLLER)
                << "Rejected block [" << encoded << "]";
            break;

        // This may have also caused blocks to be accepted via the pool.
        // The block has been accepted into the long chain (ec not set).
        case block_status::confirmed:
            log::info(LOG_POLLER)
                << "Block #" << info.height << " " << encoded;
            break;
    }
}

void poller::request_blocks(const hash_digest& stop, channel::ptr node)
{
    blockchain_.fetch_block_locator(
        std::bind(&poller::get_blocks,
            this, _1, _2, stop, node));
}

// Not having orphans will cause a stall unless mitigated.
void poller::get_blocks(const code& ec, const block_locator& locator,
    const hash_digest& stop, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    const auto start = locator.front();
    const auto encoded = encode_hash(start);

    if (ec)
    {
        log::debug(LOG_POLLER)
            << "Failure to fetch block locator for [" << node->authority()
            << "] with start [" << encoded << "]";
        return;
    }
    
    if (node->located(start, stop))
    {
        log::debug(LOG_POLLER)
            << "Skipping duplicate get blocks from [" << node->authority()
            << "] with start [" << encoded << "]";
        return;
    }
    
    log::debug(LOG_POLLER)
        << "Send get blocks to [" << node ->authority() << "] start ["
        << encoded << "](" << locator.size() << ") stop ["
        << (stop == null_hash ? "500" : encoded) << "]";

    const message::get_blocks packet{ locator, stop };

    const proxy::result_handler handler =
        std::bind(&poller::handle_get_blocks,
            this, _1, node, start, stop);

    node->send(packet, handler);
}

void poller::handle_get_blocks(const code& ec, channel::ptr node,
    const hash_digest& start, const hash_digest& stop)
{
    if (ec)
    {
        log::debug(LOG_POLLER)
            << "Failure sending get blocks to [" << node->authority()
            << "] " << ec.message();
        node->stop(ec);
        return;
    }

    // Cache last successfully-sent request identity for this channel.
    node->set_located(start, stop);
}

//// Dead code, temp retain for example.
////void poller::receive_inv(const code& ec,
////    const inventory_type& packet, channel::ptr node)
////{
////    if (ec == error::channel_stopped)
////        return;
////
////    const auto peer = node->authority();
////
////    if (ec)
////    {
////        log::debug(LOG_POLLER)
////            << "Failure in receive inventory ["
////            << peer << "] " << ec.message();
////        node->stop(ec);
////        return;
////    }
////
////    log::debug(LOG_POLLER)
////        << "Inventory BEGIN [" << peer << "] ("
////        << packet.inventories.size() << ")";
////
////    get_data_type getdata;
////    for (const auto& inventory: packet.inventories)
////    {
////        // Filter out non-block inventories
////        if (inventory.type == inventory_type_id::block)
////        {
////            log::debug(LOG_POLLER)
////                << "Block inventory from [" << peer << "] for ["
////                << encode_hash(inventory.hash) << "]";
////
////            // Exclude the last block asked for.
////            // Presumably this is because it's convenient/fast to do.
////            // TODO: filter out blocks we already have so we aren't
////            // re-requesting block data for them. Presumably we could
////            // have obtained them on another channel even though we 
////            // did ask for them on this channel.
////            if (inventory.hash != last_block_hash_)
////                getdata.inventories.push_back(inventory);
////        }
////    }
////
////    log::debug(LOG_POLLER)
////        << "Inventory END [" << peer << "]";
////
////    // The session is also subscribed to inv, so we don't handle tx here.
////    if (getdata.inventories.empty())
////    {
////        // If empty there is no ask, resulting in a stall.
////        log::debug(LOG_POLLER) 
////            << "Received empty filtered block inventory.";
////    }
////    else
////    {
////        last_block_hash_ = getdata.inventories.back().hash;
////        node->send(getdata, handle_send_packet);
////    }
////
////    // Resubscribe.
////    node->subscribe_inventory(
////        dispatch_.sync(&poller::receive_inv,
////            this, _1, _2, node));
////}

} // namespace node
} // namespace libbitcoin
