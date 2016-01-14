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
using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;

responder::responder(block_chain& blockchain, transaction_pool& tx_pool)
  : blockchain_(blockchain), tx_pool_(tx_pool)
{
}

void responder::monitor(channel::ptr node)
{
    ////// Subscribe to serve tx and blocks.
    ////node->subscribe<get_data>(
    ////    std::bind(&responder::receive_get_data,
    ////        this, _1, _2, node));
}

// TODO: consolidate to libbitcoin utils.
static size_t inventory_count(const inventory_vector::list& inventories,
    inventory_type_id type_id)
{
    size_t count = 0;

    for (const auto& inventory: inventories)
        if (inventory.type == type_id)
            ++count;

    return count;
}

// We don't seem to be getting getdata requests.
void responder::receive_get_data(const code& ec, const get_data& packet,
    channel::ptr node)
{
    if (ec == error::channel_stopped)
        return;

    const auto peer = node->authority();

    if (ec)
    {
        log::debug(LOG_RESPONDER)
            << "Failure in receive get data [" << peer << "] " << ec.message();
        node->stop(ec);
        return;
    }

    ////// Resubscribe to serve tx and blocks.
    ////node->subscribe<message::get_data>(
    ////    std::bind(&responder::receive_get_data,
    ////        this, _1, _2, node));

    log::debug(LOG_RESPONDER)
        << "Getdata BEGIN [" << peer << "] "
        << "txs (" << packet.count(inventory_type_id::transaction) << ") "
        << "blocks (" << packet.count(inventory_type_id::block) << ") "
        << "bloom (" << packet.count(inventory_type_id::filtered_block) << ")";

    for (const auto& inventory: packet.inventories)
    {
        switch (inventory.type)
        {
            case inventory_type_id::transaction:
                log::debug(LOG_RESPONDER)
                    << "Transaction getdata for [" << peer << "] "
                    << encode_hash(inventory.hash);
                tx_pool_.fetch(inventory.hash,
                    std::bind(&responder::send_pool_tx,
                        this, _1, _2, inventory.hash, node));
                break;

            case inventory_type_id::block:
                log::debug(LOG_RESPONDER)
                    << "Ignoring block getdata for [" << peer << "] "
                    << encode_hash(inventory.hash);
                ////block_fetcher::fetch(blockchain_, inventory.hash,
                ////    std::bind(&responder::send_block,
                ////        this, _1, _2, inventory.hash, node));
                break;

            case inventory_type_id::error:
            case inventory_type_id::none:
            default:
                log::debug(LOG_RESPONDER)
                    << "Ignoring invalid getdata type for [" << peer << "]";
        }
    }

    log::debug(LOG_RESPONDER)
        << "Getdata END [" << peer << "]";
}

void responder::send_pool_tx(const code& ec, const transaction& tx,
    const hash_digest& tx_hash, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_RESPONDER)
            << "Transaction for [" << node->authority()
            << "] not in mempool [" << encode_hash(tx_hash) << "]";

        // It wasn't in the mempool, so relay the request to the blockchain.
        blockchain_.fetch_transaction(tx_hash,
            std::bind(&responder::send_chain_tx,
                this, _1, _2, tx_hash, node));
        return;
    }

    if (ec)
    {
        log::error(LOG_RESPONDER)
            << "Failure fetching mempool tx data for ["
            << node->authority() << "] " << ec.message();
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
void responder::send_chain_tx(const code& ec, const transaction& tx,
    const hash_digest& tx_hash, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_RESPONDER)
            << "Transaction for [" << node->authority()
            << "] not in blockchain [" << encode_hash(tx_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_tx_not_found(tx_hash, node);
        return;
    }

    if (ec)
    {
        log::error(LOG_RESPONDER)
            << "Failure fetching blockchain tx data for ["
            << node->authority() << "] " << ec.message();
        node->stop(ec);
        return;
    }

    send_tx(tx, tx_hash, node);
}

void responder::send_tx(const transaction& tx, const hash_digest& hash,
    channel::ptr node)
{
    const auto send_handler = [hash, node](const code& ec)
    {
        if (ec)
            log::debug(LOG_RESPONDER)
                << "Failure sending tx for ["
                << node->authority() << "]";
        else
            log::debug(LOG_RESPONDER)
                << "Sent tx for [" << node->authority()
                << "] " << encode_hash(hash);
    };

    node->send(tx, send_handler);
}

void responder::send_tx_not_found(const hash_digest& hash, channel::ptr node)
{
    const auto send_handler = [hash, node](const code& ec)
    {
        if (ec)
            log::debug(LOG_RESPONDER)
                << "Failure sending tx notfound for ["
                << node->authority() << "]";
        else
            log::debug(LOG_RESPONDER)
                << "Sent tx notfound for [" << node->authority()
                << "] " << encode_hash(hash);
    };

    send_inventory_not_found(inventory_type_id::transaction, hash, node,
        send_handler);
}

// Should we look in the orphan pool first?
void responder::send_block(const code& ec, const std::shared_ptr<block> block,
    const hash_digest& block_hash, channel::ptr node)
{
    if (ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_RESPONDER)
            << "Block for [" << node->authority()
            << "] not in blockchain [" << encode_hash(block_hash) << "]";

        // It wasn't in the blockchain, so send notfound.
        send_block_not_found(block_hash, node);
    }

    if (ec)
    {
        log::error(LOG_RESPONDER)
            << "Failure fetching block data for ["
            << node->authority() << "] " << ec.message();
        node->stop(ec);
        return;
    }

    const auto send_handler = [block_hash, node](const code& ec)
    {
        if (ec)
            log::debug(LOG_RESPONDER)
                << "Failure sending block for ["
                << node->authority() << "]";
        else
            log::debug(LOG_RESPONDER)
                << "Sent block for [" << node->authority()
                << "] " << encode_hash(block_hash);
    };

    node->send(*block, send_handler);
}

void responder::send_block_not_found(const hash_digest& block_hash,
    channel::ptr node)
{
    const auto send_handler = [block_hash, node](const code& ec)
    {
        if (ec)
            log::debug(LOG_RESPONDER)
                << "Failure sending block notfound for ["
                << node->authority() << "]";
        else
            log::debug(LOG_RESPONDER)
                << "Sent block notfound for [" << node->authority()
                << "] " << encode_hash(block_hash);
    };

    send_inventory_not_found(inventory_type_id::block, block_hash,
        node, send_handler);
}

void responder::send_inventory_not_found(inventory_type_id type_id,
    const hash_digest& hash, channel::ptr node, proxy::result_handler handler)
{
    const inventory_vector block_inventory
    {
        type_id,
        hash
    };

    const not_found lost({ block_inventory });
    node->send(lost, handler);
}

} // node
} // libbitcoin
