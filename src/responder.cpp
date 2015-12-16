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
#include <bitcoin/node/responder.hpp>

#include <functional>
#include <system_error>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::chain;
using namespace bc::network;

// Respond to peer get_data and get_blocks messages.
responder::responder(blockchain& blockchain, transaction_pool& tx_pool)
  : blockchain_(blockchain),
    tx_pool_(tx_pool)
{
}

void responder::monitor(channel_ptr node)
{
    // Subscribe to serve tx, filters and blocks.
    node->subscribe_get_data(
        std::bind(&responder::receive_get_data,
            this, _1, _2, node));

    // Subscribe to new get_blocks requests.
    node->subscribe_get_blocks(
        std::bind(&responder::receive_get_blocks,
            this, _1, _2, node));
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

bool responder::receive_get_data(const std::error_code& ec,
    const get_data_type& packet, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    const auto peer = node->address();

    if (ec)
    {
        log_debug(LOG_RESPONDER)
            << "Failure in receive get_data ["
            << peer << "] " << ec.message();
        node->stop(ec);
        return false;
    }

    // TODO: we should disable all until synced.
    static const auto sending_blocks = true;
    static const auto sending_filters = false;
    static const auto sending_transactions = true;

    const auto blocks = count(packet.inventories,
        inventory_type_id::block);
    const auto filters = count(packet.inventories,
        inventory_type_id::filtered_block);
    const auto transactions = count(packet.inventories,
        inventory_type_id::transaction);

    log_debug(LOG_SESSION)
        << "Getdata BEGIN [" << peer << "] "
        << "txs (" << transactions << ") "
        << "filters (" << filters << ")"
        << "blocks (" << blocks << ")";

    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::block && sending_blocks)
        {
            log_debug(LOG_SESSION)
                << "Block get_data from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else if (inventory.type == inventory_type_id::filtered_block &&
            sending_filters)
        {
            log_debug(LOG_SESSION)
                << "Filtred block get_data from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else if (inventory.type == inventory_type_id::transaction &&
            sending_transactions)
        {
            log_debug(LOG_SESSION)
                << "Transaction get_data from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
        else
        {
            log_debug(LOG_SESSION)
                << "Ignoring invalid get_data type from [" << peer << "] "
                << encode_hash(inventory.hash);
        }
    }

    log_debug(LOG_SESSION)
        << "Getdata END [" << peer << "]";

    if (blocks > 0 && sending_blocks)
        new_block_get_data(packet, node);

    if (filters > 0 && sending_filters)
        new_filter_get_data(packet, node);

    if (transactions > 0 && sending_transactions)
        new_tx_get_data(packet, node);

    return true;
}

// It is a problem that we may not be responding in requested order.
void responder::new_block_get_data(const get_data_type& packet,
    channel_ptr node)
{
    // This doesn't test for orphan pool existence, but that should be rare.
    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::block)
        {
            chain::fetch_block(blockchain_, inventory.hash,
                std::bind(&responder::send_block,
                    this, _1, _2, inventory.hash, node));
        }
    }
}

void responder::new_filter_get_data(const get_data_type& packet,
    channel_ptr node)
{
    // we don't support filtered blocks so we shouldn't see this.
}

// It is a problem that we may not be responding in requested order.
void responder::new_tx_get_data(const get_data_type& packet, channel_ptr node)
{
    // This doesn't test for chain existence, but that should be rare.
    for (const auto& inventory: packet.inventories)
    {
        if (inventory.type == inventory_type_id::transaction)
            tx_pool_.fetch(inventory.hash,
                std::bind(&responder::send_pool_tx,
                    this, _1, _2, inventory.hash, node));
    }
}

void responder::send_pool_tx(const std::error_code& ec,
    const transaction_type& tx, const hash_digest& tx_hash, channel_ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Transaction for [" << node->address()
            << "] not in mempool [" << encode_hash(tx_hash) << "]";

        ////// It wasn't in the mempool, so relay the request to the blockchain.
        ////// This is a non-standard protocol implementation.
        ////blockchain_.fetch_transaction(tx_hash,
        ////    std::bind(&responder::send_chain_tx,
        ////        this, _1, _2, tx_hash, node));

        send_tx_not_found(tx_hash, node);
        return;
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure fetching mempool tx data for ["
            << node->address() << "] " << ec.message();
        node->stop(ec);
        return;
    }

    send_tx(tx, tx_hash, node);
}

// en.bitcoin.it/wiki/Protocol_documentation#getdata
// getdata can be used to retrieve transactions, but only if they are
// in the memory pool or relay set - arbitrary access to transactions
// in the  chain is not allowed to avoid having clients start to depend
// on nodes having full transaction indexes (which modern nodes do not).
void responder::send_chain_tx(const std::error_code& ec,
    const transaction_type& tx, const hash_digest& tx_hash, channel_ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Transaction for [" << node->address()
            << "] not in blockchain [" << encode_hash(tx_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_tx_not_found(tx_hash, node);
        return;
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure fetching blockchain tx data for ["
            << node->address() << "] " << ec.message();
        node->stop(ec);
        return;
    }

    send_tx(tx, tx_hash, node);
}

void responder::send_tx(const transaction_type& tx, const hash_digest& tx_hash,
    channel_ptr node)
{
    const auto send_handler = [tx_hash, node](const std::error_code& ec)
    {
        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx for [" << node->address() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx for [" << node->address()
                << "] " << encode_hash(tx_hash);
    };

    node->send(tx, send_handler);
}

void responder::send_tx_not_found(const hash_digest& tx_hash, channel_ptr node)
{
    const auto send_handler = [tx_hash, node](const std::error_code& ec)
    {
        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx notfound for ["
                << node->address() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx notfound for [" << node->address()
                << "] " << encode_hash(tx_hash);
    };

    send_inventory_not_found(inventory_type_id::transaction, tx_hash, node,
        send_handler);
}

// Should we look in the orphan pool first?
void responder::send_block(const std::error_code& ec, const block_type& block,
    const hash_digest& block_hash, channel_ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Block for [" << node->address()
            << "] not in blockchain [" << encode_hash(block_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_block_not_found(block_hash, node);
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure fetching block data for ["
            << node->address() << "] " << ec.message();
        node->stop(ec);
        return;
    }

    const auto send_handler = [block_hash, node](const std::error_code& ec)
    {
        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending block for [" << node->address() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent block for [" << node->address()
                << "] " << encode_hash(block_hash);
    };

    node->send(block, send_handler);
}

void responder::send_block_not_found(const hash_digest& block_hash,
    channel_ptr node)
{
    const auto send_handler = [block_hash, node](const std::error_code& ec)
    {
        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx notfound for ["
                << node->address() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx notfound for [" << node->address()
                << "] " << encode_hash(block_hash);
    };

    send_inventory_not_found(inventory_type_id::block, block_hash, node,
        send_handler);
}

void responder::send_inventory_not_found(inventory_type_id type_id,
    const hash_digest& hash, channel_ptr node,
    channel_proxy::send_handler handler)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: Implement.
    ///////////////////////////////////////////////////////////////////////////

    log_debug(LOG_RESPONDER)
        << "Failure sending notfound for [" << node->address()
        << "] feature not yet supported.";

    //const inventory_vector_type block_inventory
    //{
    //    type_id,
    //    hash
    //};

    //const get_data_type not_found{ { block_inventory } };
    //node->send(not_found, handler);
}

bool responder::receive_get_blocks(const std::error_code& ec,
    const get_blocks_type& get_blocks, channel_ptr node)
{
    if (ec == error::channel_stopped)
        return false;

    if (ec)
    {
        log_debug(LOG_SESSION)
            << "Failure in receiving get_blocks ["
            << node->address() << "] " << ec.message();
        node->stop(ec);
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: Implement.
    ///////////////////////////////////////////////////////////////////////////

    log_info(LOG_SESSION)
        << "Failure handling a get_blocks request: feature not yet supported.";

    // Resubscribe is disabled to prevent logging subsequent requests.
    return false;
}

} // node
} // libbitcoin
