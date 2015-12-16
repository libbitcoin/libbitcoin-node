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

#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/inventory.hpp>

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using namespace bc::chain;
using namespace bc::network;

// Subscribe to inventory messages and request needed blocks and txs.
// Subscribe to reorgs to set and maintain current blockchain height.
inventory::inventory(handshake& handshake, chain::blockchain& chain,
    chain::transaction_pool& tx_pool, size_t minimum_start_height)
  : handshake_(handshake),
    blockchain_(chain),
    tx_pool_(tx_pool),
    last_height_(0),
    last_block_hash_(null_hash),
    minimum_start_height_(minimum_start_height)
{
}

void inventory::monitor(channel_ptr node)
{
    // Subscribe to new inventory messages.
    node->subscribe_inventory(
        std::bind(&inventory::receive_inv,
            this, _1, _2, node));

    // Subscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&inventory::set_height,
            this, _1, _2, _3, _4));
}

static size_t count(const inventory_list& inventories,
    inventory_type_id type_id)
{
    const auto compare = [type_id](const inventory_vector_type& inventory)
    {
        return inventory.type == type_id;
    };

    return std::count_if(inventories.begin(), inventories.end(), compare);
}

bool inventory::receive_inv(const std::error_code& ec,
    const inventory_type& packet, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    const auto peer = node->address();

    if (ec)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in receive inventory ["
            << peer << "] " << ec.message();
        node->stop(ec);
        return false;
    }

    static const auto accepting_blocks = true;
    static const auto accepting_filters = false;
    const auto accepting_transactions = last_height_ >= minimum_start_height_;

    const auto blocks = count(packet.inventories,
        inventory_type_id::block);
    const auto filters = count(packet.inventories,
        inventory_type_id::filtered_block);
    const auto transactions = count(packet.inventories,
        inventory_type_id::transaction);

    log_debug(LOG_INVENTORY)
        << "Inventory BEGIN [" << peer << "] "
        << "txs (" << transactions << ") "
        << "filters (" << filters << ")"
        << "blocks (" << blocks << ")";

    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::block && accepting_blocks)
        {
            log_debug(LOG_INVENTORY)
                << "Block inventory from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else if (inventory.type == inventory_type_id::filtered_block &&
            accepting_filters)
        {
            log_debug(LOG_INVENTORY)
                << "Filtred block inventory from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else if (inventory.type == inventory_type_id::transaction &&
            accepting_transactions)
        {
            log_debug(LOG_INVENTORY)
                << "Transaction inventory from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else
        {
            log_debug(LOG_INVENTORY)
                << "Ignoring invalid inventory type from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
    }

    log_debug(LOG_INVENTORY)
        << "Inventory END [" << peer << "]";

    if (blocks > 0 && accepting_blocks)
        new_block_inventory(packet, node);

    if (filters > 0 && accepting_filters)
        new_filter_inventory(packet, node);

    if (transactions > 0 && accepting_transactions)
        new_tx_inventory(packet, node);

    return true;
}

void inventory::new_block_inventory(const inventory_type& packet,
    channel_ptr node)
{
    // last_block_hash_ is guarded by subscriber queue, only used here.

    // This doesn't test for orphan pool existence, but that should be rare.
    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::block &&
            inventory.hash != last_block_hash_)
        {
            // TODO: add blockchain.fetch_difference method.
            // TODO: pass array of hashes and return those that do not exist.
            blockchain_.fetch_block_header(inventory.hash,
                std::bind(&inventory::get_block,
                    this, _1, inventory.hash, node));
        }
    }

    ////last_block_hash_ = getdata.inventories.back().hash;
}

void inventory::new_filter_inventory(const inventory_type& packet,
    channel_ptr node)
{
    // we don't support filtered blocks so we shouldn't see this.
}

void inventory::new_tx_inventory(const inventory_type& packet, channel_ptr node)
{
    // If the tx doesn't exist in our mempool, issue getdata.
    // This doesn't test for chain existence, but that should be rare.
    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::transaction)
        {
            // TODO: add transaction_pool.fetch_difference method.
            // TODO: pass array of hashes and return those that do not exist.
            tx_pool_.exists(inventory.hash,
                std::bind(&inventory::get_tx,
                    this, _1, _2, inventory.hash, node));
        }
    }
}

void inventory::get_tx(const std::error_code& ec, bool exists,
    const hash_digest& hash, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in getting transaction existence ["
            << encode_hash(hash) << "] " << ec.message();
        return;
    }

    if (exists)
    {
        log_debug(LOG_INVENTORY)
            << "Transaction already exists [" << encode_hash(hash) << "]";
        return;
    }

    const auto handle_error = [node](const std::error_code& ec)
    {
        if (ec)
        {
            log_debug(LOG_INVENTORY)
                << "Failure sending tx data request to [" 
                << node->address() << "] " << ec.message();
            node->stop(ec);
            return;
        }
    };

    log_debug(LOG_INVENTORY)
        << "Requesting transaction [" << encode_hash(hash) << "]";

    const inventory_vector_type inventory
    {
        inventory_type_id::transaction,
        hash
    };

    const get_data_type request{ { inventory } };
    node->send(request, handle_error);
}

void inventory::get_block(const std::error_code& ec, const hash_digest& hash,
    channel_ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec && ec != error::not_found)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in getting block existence ["
            << encode_hash(hash) << "] " << ec.message();
        return;
    }

    if (!ec)
    {
        log_debug(LOG_INVENTORY)
            << "Block already exists [" << encode_hash(hash) << "]";
        return;
    }

    const auto handle_error = [node](const std::error_code& ec)
    {
        if (ec)
        {
            log_debug(LOG_INVENTORY)
                << "Failure sending block data request to [" 
                << node->address() << "] " << ec.message();
            node->stop(ec);
            return;
        }
    };

    log_debug(LOG_INVENTORY)
        << "Requesting block [" << encode_hash(hash) << "]";

    const inventory_vector_type inventory
    {
        inventory_type_id::block,
        hash
    };

    const get_data_type request{ { inventory } };
    node->send(request, handle_error);
}

bool inventory::set_height(const std::error_code& ec,
    uint32_t fork_point, const blockchain::block_list& new_blocks,
    const blockchain::block_list& /* replaced_blocks */)
{
    if (ec == error::service_stopped)
        return false;

    if (ec)
    {
        log_error(LOG_INVENTORY)
            << "Failure handling reorg: " << ec.message();
        return false;
    }

    // Start height is limited to max_uint32 by satoshi protocol (version).
    BITCOIN_ASSERT((bc::max_uint32 - fork_point) >= new_blocks.size());
    const auto height = static_cast<uint32_t>(fork_point + new_blocks.size());

    handshake_.set_start_height(height, 
        std::bind(&inventory::handle_set_height,
            this, _1, height));

    return true;
}

void inventory::handle_set_height(const std::error_code& ec, uint32_t height)
{
    if (ec)
    {
        log_error(LOG_INVENTORY)
            << "Failure setting start height: " << ec.message();
        return;
    }

    // atomic
    last_height_ = height;

    log_debug(LOG_INVENTORY)
        << "Reorg set start height [" << height << "]";
}

} // namespace node
} // namespace libbitcoin
