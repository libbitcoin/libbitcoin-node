/*
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
using namespace bc::blockchain;
using namespace bc::network;

responder::responder(bc::blockchain::blockchain& blockchain,
    bc::blockchain::transaction_pool& tx_pool)
  : blockchain_(blockchain), tx_pool_(tx_pool)
{
}

void responder::monitor(channel::pointer node)
{
    if (!node)
        return;

    // Subscribe to serve tx and blocks.
    node->subscribe_get_data(
        std::bind(&responder::receive_get_data,
            this, _1, _2, node));
}

// TODO: consolidate to libbitcoin utils.
static size_t inventory_count(const message::inventory_list& inventories,
    message::inventory_type_id type_id)
{
    size_t count = 0;

    for (const auto& inventory: inventories)
        if (inventory.type == type_id)
            ++count;

    return count;
}

// We don't seem to be getting getdata requests.
void responder::receive_get_data(const std::error_code& ec,
    const message::get_data& packet, channel::pointer node)
{
    if (!node)
        return;

    const auto peer = node->address().to_string();

    if (ec)
    {
        log_debug(LOG_RESPONDER)
            << "Failure in receive get data ["
            << peer << "] " << ec.message();
        node->stop();
        return;
    }

    const auto blocks = inventory_count(packet.inventories,
        message::inventory_type_id::block);
    const auto transactions = inventory_count(packet.inventories,
        message::inventory_type_id::transaction);

    log_debug(LOG_RESPONDER)
        << "Getdata BEGIN [" << peer << "] "
        << "txs (" << transactions << ") "
        << "blocks (" << blocks << ")";

    for (const auto& inventory: packet.inventories)
    {
        switch (inventory.type)
        {
            case message::inventory_type_id::transaction:
                log_debug(LOG_RESPONDER)
                    << "Transaction inventory for [" << peer << "] "
                    << encode_hash(inventory.hash);
                tx_pool_.fetch(inventory.hash,
                    std::bind(&responder::send_pool_tx,
                        this, _1, _2, inventory.hash, node));
                break;

            case message::inventory_type_id::block:
                log_debug(LOG_RESPONDER)
                    << "Block inventory for [" << peer << "] "
                    << encode_hash(inventory.hash);
                fetch_block(blockchain_, inventory.hash,
                    std::bind(&responder::send_block,
                        this, _1, _2, inventory.hash, node));
                break;

            case message::inventory_type_id::error:
            case message::inventory_type_id::none:
            default:
                log_debug(LOG_RESPONDER)
                    << "Ignoring invalid inventory type for [" << peer << "]";
        }
    }

    log_debug(LOG_RESPONDER)
        << "Inventory END [" << peer << "]";

    // Node may have died following tx_pool_.fetch or fetch_block.
    if (!node)
        return;

    // Resubscribe to serve tx and blocks.
    node->subscribe_get_data(
        std::bind(&responder::receive_get_data,
            this, _1, _2, node));
}

void responder::send_pool_tx(const std::error_code& ec,
    const chain::transaction& tx, const hash_digest& tx_hash, channel::pointer node)
{
    if (!node)
        return;

    // TODO: confirm error code.
    if (ec == bc::error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Transaction for [" << node->address().to_string()
            << "] not in mempool [" << encode_hash(tx_hash) << "]";

        // It wasn't in the mempool, so relay the request to the blockchain.
        blockchain_.fetch_transaction(tx_hash,
            std::bind(&responder::send_chain_tx,
                this, _1, _2, tx_hash, node));
        return;
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure to get mempool tx data for ["
            << node->address().to_string() << "] " << ec.message();
        node->stop();
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
    const chain::transaction& tx, const hash_digest& tx_hash,
    channel::pointer node)
{
    if (!node)
        return;

    // TODO: confirm error code.
    if (ec == bc::error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Transaction for [" << node->address().to_string()
            << "] not in blockchain [" << encode_hash(tx_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_tx_not_found(tx_hash, node);
        return;
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure to get blockchain tx data for ["
            << node->address().to_string() << "] " << ec.message();
        node->stop();
        return;
    }

    send_tx(tx, tx_hash, node);
}

void responder::send_tx(const chain::transaction& tx,
    const hash_digest& tx_hash, channel::pointer node)
{
    if (!node)
        return;

    const auto send_handler = [tx_hash, node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx for ["
                << node->address().to_string() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx for [" << node->address().to_string()
                << "] " << encode_hash(tx_hash);
    };

    node->send(tx, send_handler);
}

void responder::send_tx_not_found(const hash_digest& tx_hash, channel::pointer node)
{
    const auto send_handler = [tx_hash, node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx notfound for ["
                << node->address().to_string() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx notfound for [" << node->address().to_string()
                << "] " << encode_hash(tx_hash);
    };

    send_inventory_not_found(message::inventory_type_id::transaction, tx_hash,
        node, send_handler);
}

// Should we look in the orphan pool first?
void responder::send_block(const std::error_code& ec,
    const chain::block& block, const hash_digest& block_hash,
    channel::pointer node)
{
    if (!node)
        return;

    // TODO: verify error code.
    if (ec == bc::error::not_found)
    {
        log_debug(LOG_RESPONDER)
            << "Block for [" << node->address().to_string()
            << "] not in blockchain [" << encode_hash(block_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_block_not_found(block_hash, node);
    }

    if (ec)
    {
        log_error(LOG_RESPONDER)
            << "Failure to get block data for ["
            << node->address().to_string() << "] " << ec.message();
        node->stop();
        return;
    }

    const auto send_handler = [block_hash, node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending block for ["
                << node->address().to_string() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent block for [" << node->address().to_string()
                << "] " << encode_hash(block_hash);
    };

    node->send(block, send_handler);
}

void responder::send_block_not_found(const hash_digest& block_hash,
    channel::pointer node)
{
    const auto send_handler = [block_hash, node](const std::error_code& ec)
    {
        if (!node)
            return;

        if (ec)
            log_debug(LOG_RESPONDER)
                << "Failure sending tx notfound for ["
                << node->address().to_string() << "]";
        else
            log_debug(LOG_RESPONDER)
                << "Sent tx notfound for [" << node->address().to_string()
                << "] " << encode_hash(block_hash);
    };

    send_inventory_not_found(message::inventory_type_id::block, block_hash,
        node, send_handler);
}

void responder::send_inventory_not_found(message::inventory_type_id type_id,
    const hash_digest& hash, channel::pointer node,
    channel_proxy::send_handler handler)
{
    if (!node)
        return;

    // There's currently no way to send this message.
    //const inventory_vector_type block_inventory
    //{
    //    type_id,
    //    hash
    //};

    //const get_data_type not_found{ { block_inventory } };
    //node->send(not_found, handler);
}

} // node
} // libbitcoin
