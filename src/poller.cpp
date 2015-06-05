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

poller::poller(threadpool& pool, blockchain& chain)
  : strand_(pool),
    blockchain_(chain),
    last_block_hash_(null_hash),
    last_locator_begin_(null_hash),
    last_hash_stop_(null_hash), 
    last_requested_node_(nullptr)
{
}

void poller::query(channel_ptr node)
{
    fetch_block_locator(blockchain_,
        std::bind(&poller::initial_ask_blocks,
            this, _1, _2, node));
}

void poller::monitor(channel_ptr node)
{
    node->subscribe_inventory(
        strand_.wrap(&poller::receive_inv,
            this, _1, _2, node));

    node->subscribe_block(
        std::bind(&poller::receive_block,
            this, _1, _2, node));
}

void poller::initial_ask_blocks(const std::error_code& ec,
    const block_locator_type& locator, channel_ptr node)
{
    if (ec)
    {
        log_error(LOG_POLLER) << "Fetching initial block locator: "
            << ec.message();
        return;
    }

    strand_.randomly_queue(
        &poller::ask_blocks,
            this, ec, locator, null_hash, node);
}

void handle_send_packet(const std::error_code& ec)
{
    if (ec)
        log_error(LOG_POLLER) << "Send problem: "
            << ec.message();
}

void poller::receive_inv(const std::error_code& ec,
    const inventory_type& packet, channel_ptr node)
{
    if (ec)
    {
        log_warning(LOG_POLLER) << "Received bad inventory: "
            << ec.message();
        return;
    }

    get_data_type getdata;
    for (const inventory_vector_type& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::block &&
            inventory.hash != last_block_hash_)
        {
            // Filter out only block inventories
            getdata.inventories.push_back(inventory);
        }
    }

    if (!getdata.inventories.empty())
    {
        last_block_hash_ = getdata.inventories.back().hash;
        node->send(getdata, handle_send_packet);
    }

    node->subscribe_inventory(
        strand_.wrap(&poller::receive_inv,
            this, _1, _2, node));
}

void poller::receive_block(const std::error_code& ec,
    const block_type& block, channel_ptr node)
{
    if (ec)
    {
        log_warning(LOG_POLLER) << "Received bad block: "
            << ec.message();
        return;
    }

    blockchain_.store(block,
        strand_.wrap(&poller::handle_store,
            this, _1, _2, hash_block_header(block.header), node));

    BITCOIN_ASSERT(node);
    node->subscribe_block(
        std::bind(&poller::receive_block,
            this, _1, _2, node));
}

void poller::handle_store(const std::error_code& ec, block_info info,
    const hash_digest& block_hash, channel_ptr node)
{
    if (ec == error::duplicate /*&& info.status == block_status::rejected*/)
    {
        // This is common, we get blocks we already have.
        log_debug(LOG_POLLER) << "Redundant block " 
            << encode_hash(block_hash);
        return;
    }

    // We need orphan blocks so we can do the next getblocks round.
    // So ec should not be set when info.status is block_status::orphan.
    if (ec /*&& info.status != block_status::orphan*/)
    {
        log_error(LOG_POLLER) << "Error storing block "
            << encode_hash(block_hash) << ": " << ec.message();
        return;
    }

    switch (info.status)
    {
        // The block has been accepted as an orphan (ec not set).
        case block_status::orphan:
            log_debug(LOG_POLLER) << "Potential block " 
                << encode_hash(block_hash);

            // TODO: Make more efficient by storing block hash
            // and next time do not download orphan block again.
            // Remember to remove from list once block is no longer orphan.
            fetch_block_locator(blockchain_,
                strand_.wrap(&poller::ask_blocks, 
                    this, _1, _2, block_hash, node));

            break;

        // The block has been rejected from the store.
        // TODO: determine if ec is also set, since that is already handled.
        case block_status::rejected:
            log_warning(LOG_POLLER) << "Rejected block " 
                << encode_hash(block_hash);
            break;

        // The block has been accepted into the long chain (ec not set).
        case block_status::confirmed:
            log_info(LOG_POLLER) << "Block #" 
                << info.height << " " << encode_hash(block_hash);
            break;
    }
}

void poller::ask_blocks(const std::error_code& ec,
    const block_locator_type& locator, const hash_digest& hash_stop,
    channel_ptr node)
{
    if (ec)
    {
        log_error(LOG_POLLER) << "Ask for blocks: "
            << ec.message();
        return;
    }
    
    if (last_locator_begin_ == locator.front() &&
        last_hash_stop_ == hash_stop && last_requested_node_ == node.get())
    {
        log_debug(LOG_POLLER) << "Skipping duplicate ask blocks: "
            << encode_hash(locator.front());
        return;
    }

    // Send get_blocks request.
    get_blocks_type packet;
    packet.start_hashes = locator;
    packet.hash_stop = hash_stop;
    node->send(packet, std::bind(&handle_send_packet, _1));

    // Update last values.
    last_locator_begin_ = locator.front();
    last_hash_stop_ = hash_stop;
    last_requested_node_ = node.get();
}

} // namespace node
} // namespace libbitcoin

