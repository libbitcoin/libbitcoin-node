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
    minimum_start_height_(minimum_start_height)
{
}

// Inventory utilities
// ----------------------------------------------------------------------------

std::string inventory::to_text(inventory_type_id type)
{
    switch (type)
    {
    case inventory_type_id::block:
        return "block";
    case inventory_type_id::transaction:
        return "transaction";
    case inventory_type_id::filtered_block:
        return "filtered_block";
    case inventory_type_id::error:
        return "error";
    case inventory_type_id::none:
        return "none";
    default:
        return "undefined";
    }
};

hash_list inventory::to_hashes(const inventory_list& inventories,
    inventory_type_id type)
{
    hash_list hashes;
    for (const auto& inventory: inventories)
        if (inventory .type == type)
            hashes.push_back(inventory.hash);

    return hashes;
}

inventory_list inventory::to_inventories(const hash_list& hashes,
    inventory_type_id type)
{
    inventory_list inventories;
    for (const auto& hash: hashes)
        inventories.push_back({ type, hash });

    return inventories;
}

inventory_list inventory::to_inventories(const blockchain::block_list& blocks)
{
    static const auto type = inventory_type_id::block;

    inventory_list inventories;
    for (const auto block: blocks)
        inventories.push_back({ type, hash_block_header(block->header) });

    return inventories;
}

size_t inventory::count(const inventory_list& inventories,
    inventory_type_id type_id)
{
    const auto compare = [type_id](const inventory_vector_type& inventory)
    {
        return inventory.type == type_id;
    };

    return std::count_if(inventories.begin(), inventories.end(), compare);
}

// This should be implemented on a per-channel basis, if at all.
////// Remove the last from the inventory list store the last from this list.
////void inventory::sanitize_block_hashes(hash_list& hashes)
////{
////    // last_block_hash_ used only here, guarded by non-concurrency guarantee.
////    auto it = std::find(hashes.begin(), hashes.end(), last_block_hash_);
////
////    if (it != hashes.end())
////        hashes.erase(it);
////
////    if (!hashes.empty())
////        last_block_hash_ = hashes.back();
////}

// Startup
// ----------------------------------------------------------------------------

void inventory::monitor(channel_ptr node)
{
    // Subscribe to inventory messages.
    node->subscribe_inventory(
        std::bind(&inventory::receive_inv,
            this, _1, _2, node));

    // Subscribe to reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&inventory::handle_reorg,
            this, _1, _2, _3, _4));
}

// Handle inventory message
// ----------------------------------------------------------------------------

bool inventory::receive_inv(const std::error_code& ec,
    const inventory_type& packet, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    const auto peer = node->address();

    if (ec)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in receive inventory [" << peer << "] "
            << ec.message();
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
        << "filters (" << filters << ") "
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
            log_debug(LOG_SESSION)
                << "Ignoring " << inventory::to_text(inventory.type)
                << " inventory type from [" << peer << "] "
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
        new_transaction_inventory(packet, node);

    return true;
}

// Blocks
// ----------------------------------------------------------------------------

// Issue get_data for the missing blocks.
// This doesn't test for orphan pool existence, but that's ok to miss.
void inventory::new_block_inventory(const inventory_type& packet,
    channel_ptr node)
{
    auto blocks = to_hashes(packet.inventories, inventory_type_id::block);
    ////sanitize_block_hashes(blocks);

    blockchain_.fetch_missing_block_hashes(blocks,
        std::bind(&inventory::get_blocks,
            this, _1, _2, node));
}

void inventory::get_blocks(const std::error_code& ec, const hash_list& hashes,
    channel_ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in getting block existence for peer "
            << node->address() << "] " << ec.message();
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
        << "Requesting " << hashes.size() << " blocks from ["
        << node->address() << "]";

    const get_data_type request
    {
        to_inventories(hashes, inventory_type_id::block)
    };

    node->send(request, handle_error);
}

// Transactions
// ----------------------------------------------------------------------------

// Issue get_data for the missing transactions.
// This doesn't test for chain existence, but that's ok to miss.
void inventory::new_transaction_inventory(const inventory_type& packet,
    channel_ptr node)
{
    const auto transactions = to_hashes(packet.inventories,
        inventory_type_id::transaction);

    tx_pool_.fetch_missing_hashes(transactions,
        std::bind(&inventory::get_transactions,
            this, _1, _2, node));
}

void inventory::get_transactions(const std::error_code& ec,
    const hash_list& hashes, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return;

    if (ec)
    {
        log_debug(LOG_INVENTORY)
            << "Failure in getting tx existence for peer "
            << node->address() << "] " << ec.message();
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
        << "Requesting " << hashes.size() << " txs from ["
        << node->address() << "]";

    const get_data_type request
    {
        to_inventories(hashes, inventory_type_id::transaction)
    };

    node->send(request, handle_error);
}

// Filters (not supported)
// ----------------------------------------------------------------------------

// We don't support filtered blocks so we shouldn't see this.
void inventory::new_filter_inventory(const inventory_type& packet,
    channel_ptr node)
{
}

// Handle reorganization (set height)
// ----------------------------------------------------------------------------

bool inventory::handle_reorg(const std::error_code& ec, uint32_t fork_point,
    const blockchain::block_list& new_blocks, const blockchain::block_list&)
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

    // TODO: configure handshake with minimum_start_height so that it can
    // disable relay from new peers until synced to the last checkpoint.
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
}

} // namespace node
} // namespace libbitcoin
