/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_transaction_out.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <boost/range/adaptor/reversed.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "transaction_out"
#define CLASS protocol_transaction_out

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::database;
using namespace bc::message;
using namespace bc::network;
using namespace boost::adaptors;
using namespace std::placeholders;

inline bool is_witness(uint64_t services)
{
    return (services & version::service::node_witness) != 0;
}

protocol_transaction_out::protocol_transaction_out(full_node& network,
    channel::ptr channel, safe_chain& chain)
  : protocol_events(network, channel, NAME),
    chain_(chain),

    // TODO: move fee filter to a derived class protocol_transaction_out_70013.
    minimum_peer_fee_(0),

    // TODO: move relay to a derived class protocol_transaction_out_70001.
    relay_to_peer_(peer_version()->relay()),

    // Witness requests must be allowed if advertising the service.
    enable_witness_(is_witness(network.network_settings().services)),
    CONSTRUCT_TRACK(protocol_transaction_out)
{
}

// TODO: move not_found to derived class protocol_transaction_out_70001.

// Start.
//-----------------------------------------------------------------------------

void protocol_transaction_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    // TODO: move relay to a derived class protocol_transaction_out_70001.
    // Prior to this level transaction relay is not configurable.
    if (relay_to_peer_)
    {
        // Subscribe to transaction pool notifications and relay txs.
        chain_.subscribe_transactions(BIND2(handle_transaction_pool, _1, _2));
    }

    // TODO: move fee filter to a derived class protocol_transaction_out_70013.
    SUBSCRIBE2(fee_filter, handle_receive_fee_filter, _1, _2);

    // TODO: move memory pool to a derived class protocol_transaction_out_60002.
    SUBSCRIBE2(memory_pool, handle_receive_memory_pool, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);
}

// Receive send_headers.
//-----------------------------------------------------------------------------

// TODO: move fee_filters to a derived class protocol_transaction_out_70013.
bool protocol_transaction_out::handle_receive_fee_filter(const code& ec,
    fee_filter_const_ptr message)
{
    if (stopped(ec))
        return false;

    // TODO: move fee filter to a derived class protocol_transaction_out_70013.
    // Transaction annoucements will be filtered by fee amount.
    minimum_peer_fee_ = message->minimum_fee();

    // The fee filter may be adjusted.
    return true;
}

// Receive mempool sequence.
//-----------------------------------------------------------------------------

// TODO: move memory_pool to a derived class protocol_transaction_out_60002.
bool protocol_transaction_out::handle_receive_memory_pool(const code& ec,
    memory_pool_const_ptr)
{
    if (stopped(ec))
        return false;

    // The handler may be invoked *multiple times* by one blockchain call.
    // TODO: move multiple mempool inv to protocol_transaction_out_70002.
    // TODO: move fee filter to a derived class protocol_transaction_out_70013.
    chain_.fetch_mempool(max_inventory, minimum_peer_fee_,
        BIND2(handle_fetch_mempool, _1, _2));

    // Drop this subscription after the first request.
    return false;
}

// Each invocation is limited to 50000 vectors and invoked from common thread.
void protocol_transaction_out::handle_fetch_mempool(const code& ec,
    inventory_ptr message)
{
    if (stopped(ec) || message->inventories().empty())
        return;

    SEND2(*message, handle_send, _1, message->command);
}

// Receive get_data sequence.
//-----------------------------------------------------------------------------

// THIS SUPPORTS REQUEST OF CONFIRMED TRANSACTIONS.
// TODO: subscribe to and handle get_block_transactions message.
// TODO: expose a new service bit that indicates complete current tx history.
// This would exclude transctions replaced by duplication as per BIP30.
bool protocol_transaction_out::handle_receive_get_data(const code& ec,
    get_data_const_ptr message)
{
    if (stopped(ec))
        return false;

    // Create a copy because message is const because it is shared.
    const auto response = std::make_shared<inventory>();

    // Reverse copy the transaction elements of the const inventory.
    for (const auto inventory: reverse(message->inventories()))
        if (inventory.is_transaction_type())
            response->inventories().push_back(inventory);

    send_next_data(response);
    return true;
}

void protocol_transaction_out::send_next_data(inventory_ptr inventory)
{
    if (inventory->inventories().empty())
        return;

    // The order is reversed so that we can pop from the back.
    const auto& entry = inventory->inventories().back();

    switch (entry.type())
    {
        case inventory::type_id::witness_transaction:
        {
            if (!enable_witness_)
            {
                stop(error::channel_stopped);
                return;
            }

            chain_.fetch_transaction(entry.hash(), false, true,
                BIND5(send_transaction, _1, _2, _3, _4, inventory));
            break;
        }
        case inventory::type_id::transaction:
        {
            chain_.fetch_transaction(entry.hash(), false, false,
                BIND5(send_transaction, _1, _2, _3, _4, inventory));
            break;
        }
        default:
        {
            BITCOIN_ASSERT_MSG(false, "improperly-filtered inventory");
        }
    }
}

// TODO: send block_transaction message as applicable.
void protocol_transaction_out::send_transaction(const code& ec,
    transaction_const_ptr message, size_t position, size_t /*height*/,
    inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    // Treat already confirmed transactions as not found.
    auto confirmed = !ec && position != transaction_result::unconfirmed;

    if (ec == error::not_found || confirmed)
    {
        LOG_DEBUG(LOG_NODE)
            << "Transaction requested by [" << authority() << "] not found.";

        // TODO: move not_found to derived class protocol_block_out_70001.
        BITCOIN_ASSERT(!inventory->inventories().empty());
        const not_found reply{ inventory->inventories().back() };
        SEND2(reply, handle_send, _1, reply.command);
        handle_send_next(error::success, inventory);
        return;
    }

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating transaction requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*message, handle_send_next, _1, inventory);
}

void protocol_transaction_out::handle_send_next(const code& ec,
    inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    BITCOIN_ASSERT(!inventory->inventories().empty());
    inventory->inventories().pop_back();

    // Break off recursion.
    DISPATCH_CONCURRENT1(send_next_data, inventory);
}

// Subscription.
//-----------------------------------------------------------------------------

bool protocol_transaction_out::handle_transaction_pool(const code& ec,
    transaction_const_ptr message)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure handling transaction notification: " << ec.message();
        stop(ec);
        return false;
    }

    // TODO: make this a collection and send empty in this case.
    // Nothing to do, a channel is stopping but it's not this one.
    if (!message)
        return true;

    // Do not announce transactions to peer if too far behind.
    // Typically the tx would not validate anyway, but this is more consistent.
    if (chain_.is_blocks_stale())
        return true;

    if (message->metadata.originator == nonce())
        return true;

    // TODO: move fee_filter to a derived class protocol_transaction_out_70013.
    if (message->fees() < minimum_peer_fee_)
        return true;

    static const auto id = inventory::type_id::transaction;
    const inventory announce{ { id, message->hash() } };
    SEND2(announce, handle_send, _1, announce.command);

    ////LOG_DEBUG(LOG_NODE)
    ////    << "Announced tx [" << encode_hash(message->hash()) << "] to ["
    ////    << authority() << "].";
    return true;
}

void protocol_transaction_out::handle_stop(const code&)
{
    chain_.unsubscribe();

    LOG_VERBOSE(LOG_NETWORK)
        << "Stopped transaction_out protocol for [" << authority() << "].";
}

} // namespace node
} // namespace libbitcoin
