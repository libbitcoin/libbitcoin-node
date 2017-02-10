/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_transaction_in.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "transaction_in"
#define CLASS protocol_transaction_in

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_transaction_in::protocol_transaction_in(full_node& node,
    channel::ptr channel, safe_chain& chain)
  : protocol_events(node, channel, NAME),
    chain_(chain),

    // TODO: move relay to a derived class protocol_transaction_in_70001.
    relay_from_peer_(node.network_settings().relay_transactions),

    // TODO: move memory_pool to a derived class protocol_transaction_in_60002.
    refresh_pool_(negotiated_version() >= version::level::bip35 &&
        node.node_settings().refresh_transactions),

    // TODO: move fee_filter to a derived class protocol_transaction_in_70013.
    minimum_relay_fee_(negotiated_version() >= version::level::bip133 ?
        node.chain_settings().minimum_relay_fee_satoshis : 0),
    CONSTRUCT_TRACK(protocol_transaction_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_transaction_in::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    // TODO: move fee_filter to a derived class protocol_transaction_in_70013.
    if (minimum_relay_fee_ != 0)
    {
        // Have the peer filter the transactions it announces to us.
        SEND2(fee_filter{ minimum_relay_fee_ }, handle_send, _1,
            fee_filter::command);
    }

    // TODO: move memory_pool to a derived class protocol_transaction_in_60002.
    if (refresh_pool_ && relay_from_peer_)
    {
        // Refresh transaction pool on connect.
        SEND2(memory_pool(), handle_send, _1, memory_pool::command);
    }

    SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    SUBSCRIBE2(transaction, handle_receive_transaction, _1, _2);
}

// Receive inventory sequence.
//-----------------------------------------------------------------------------

bool protocol_transaction_in::handle_receive_inventory(const code& ec,
    inventory_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto response = std::make_shared<get_data>();

    // Copy the transaction inventories into a get_data instance.
    message->reduce(response->inventories(), inventory::type_id::transaction);

    // TODO: move relay to a derived class protocol_transaction_in_70001.
    // Prior to this level transaction relay is not configurable.
    if (!relay_from_peer_ && !response->inventories().empty())
    {
        LOG_WARNING(LOG_NODE)
            << "Unexpected transaction inventory from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    // Remove hashes of (unspent) transactions that we already have.
    // BUGBUG: this removes spent transactions which it should not (see BIP30).
    chain_.filter_transactions(response, BIND2(send_get_data, _1, response));
    return true;
}

void protocol_transaction_in::send_get_data(const code& ec,
    get_data_ptr message)
{
    if (stopped(ec) || message->inventories().empty())
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure filtering transaction hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // inventory->get_data[transaction]
    SEND2(*message, handle_send, _1, message->command);
}

// Receive transaction sequence.
//-----------------------------------------------------------------------------

bool protocol_transaction_in::handle_receive_transaction(const code& ec,
    transaction_const_ptr message)
{
    if (stopped(ec))
        return false;

    // TODO: move relay to a derived class protocol_transaction_in_70001.
    // Prior to this level transaction relay is not configurable.
    if (!relay_from_peer_)
    {
        LOG_DEBUG(LOG_NODE)
            << "Unexpected transaction relay from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    message->validation.originator = nonce();
    chain_.organize(message, BIND2(handle_store_transaction, _1, message));
    return true;
}

// The transaction has been saved to the memory pool (or not).
// This will be picked up by subscription in transaction_out and will cause
// the transaction to be announced to non-originating relay-accepting peers.
void protocol_transaction_in::handle_store_transaction(const code& ec,
    transaction_const_ptr message)
{
    if (stopped(ec))
        return;

    // Ask the peer for ancestor txs if this one is an orphan.
    // We may not get this transaction back, but that is a fair tradeoff for
    // not having to store potentially invalid transactions.
    if (ec == error::orphan_transaction)
        send_get_transactions(message);

    const auto encoded = encode_hash(message->hash());

    // It is okay for us to receive a duplicate or a missing outputs tx but it
    // is not generally okay to receive an otherwise invalid transaction.
    // Below-fee transactions can be sent prior to fee_filter receipt or due to
    // a negotiated version below BIP133 (7013).

    // TODO: differentiate failure conditions and send reject as applicable.

    if (ec)
    {
        // This should not happen with a single peer since we filter inventory.
        // However it will happen when a block or another peer's tx intervenes.
        LOG_DEBUG(LOG_NODE)
            << "Dropped transaction [" << encoded << "] from [" << authority()
            << "] " << ec.message();
        return;
    }

    LOG_DEBUG(LOG_NODE)
        << "Stored transaction [" << encoded << "] from [" << authority()
        << "].";
}

// This will get chatty if the peer sends mempool response out of order.
// This requests the next level of missing tx, but those may be orphans as
// well. Those would also be discarded, until arriving at connectable txs.
// There is no facility to re-obtain the dropped transactions. They must be
// obtained coincidentally by peers, by a mempool message (new channel) or as a
// result of transaction resends.
void protocol_transaction_in::send_get_transactions(
    transaction_const_ptr message)
{
    static const auto type = inventory::type_id::transaction;
    auto missing = message->missing_previous_transactions();

    if (missing.empty())
        return;

    const auto request = std::make_shared<get_data>(std::move(missing), type);

    // Remove hashes of (unspent) transactions that we already have.
    // This removes spent transactions which is not correnct, however given the
    // treatment of duplicate hashes by other nodes and the fact that this is
    // a set of pool transactions only, this is okay.
    chain_.filter_transactions(request, BIND2(send_get_data, _1, request));
}

// Stop.
//-----------------------------------------------------------------------------

void protocol_transaction_in::handle_stop(const code&)
{
    LOG_DEBUG(LOG_NETWORK)
        << "Stopped transaction_in protocol for [" << authority() << "].";
}

} // namespace node
} // namespace libbitcoin
