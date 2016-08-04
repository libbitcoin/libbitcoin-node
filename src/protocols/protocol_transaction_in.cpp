/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_transaction_in.hpp>

#include <cstddef>
#include <functional>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "transaction"
#define CLASS protocol_transaction_in

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_transaction_in::protocol_transaction_in(p2p& network,
    channel::ptr channel, block_chain& blockchain, transaction_pool& pool)
  : protocol_events(network, channel, NAME),
    blockchain_(blockchain),
    pool_(pool),
    relay_from_peer_(network.network_settings().relay_transactions),
    CONSTRUCT_TRACK(protocol_transaction_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_transaction_in::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    if (relay_from_peer_ && peer_supports_memory_pool_message())
    {
        // This is not a protocol requirement, just our behavior.
        SEND2(memory_pool(), handle_send, _1, memory_pool::command);
    }

    SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    SUBSCRIBE2(transaction, handle_receive_transaction, _1, _2);
}

// Receive inventory sequence.
//-----------------------------------------------------------------------------

bool protocol_transaction_in::handle_receive_inventory(const code& ec,
    message::inventory::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting inventory from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list transaction_hashes;
    message->to_hashes(transaction_hashes, inventory_type_id::transaction);

    if (!relay_from_peer_ && !transaction_hashes.empty())
    {
        log::debug(LOG_NODE)
            << "Unexpected transaction inventory from [" << authority()
            << "] " << ec.message();
        stop(ec);
        return false;
    }

    // TODO: implement pool_.fetch_missing_transaction_hashes(...)
    send_get_data(error::success, transaction_hashes);
    return true;
}

void protocol_transaction_in::send_get_data(const code& ec,
    const hash_list& hashes)
{
    if (stopped() || ec == error::service_stopped || hashes.empty())
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating missing transaction hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // inventory->get_data[transaction]
    get_data request(hashes, inventory_type_id::transaction);
    SEND2(request, handle_send, _1, request.command);
}

// Receive transaction sequence.
//-----------------------------------------------------------------------------

bool protocol_transaction_in::handle_receive_transaction(const code& ec,
    message::transaction::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting transaction from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    if (!relay_from_peer_)
    {
        log::debug(LOG_NODE)
            << "Unexpected transaction from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    log::debug(LOG_NODE)
        << "Potential transaction from [" << authority() << "].";

    pool_.store(*message,
        BIND3(handle_store_confirmed, _1, _2, _3),
        BIND4(handle_store_validated, _1, _2, _3, _4));
    return true;
}

// The transaction has been saved to the memory pool (or not).
void protocol_transaction_in::handle_store_validated(const code& ec,
    const transaction& tx, const hash_digest& hash,
    const chain::point::indexes& unconfirmed)
{
    // Examples:
    // error::service_stopped
    // error::input_not_found
    // error::validate_inputs_failed
    // error::duplicate
}

// The transaction has been confirmed in a block.
void protocol_transaction_in::handle_store_confirmed(const code& ec,
    const transaction& tx, const hash_digest& hash)
{
    // Examples:
    // error::service_stopped
    // error::pool_filled
    // error::double_spend
    // error::blockchain_reorganized
}

// Stop.
//-----------------------------------------------------------------------------

void protocol_transaction_in::handle_stop(const code&)
{
    log::debug(LOG_NETWORK)
        << "Stopped transaction_in protocol";
}

// Utility.
//-----------------------------------------------------------------------------

bool protocol_transaction_in::peer_supports_memory_pool_message()
{
    // TODO: move mempool supprt test to enum and version method.
    return peer_version().value >= 70002;
}

} // namespace node
} // namespace libbitcoin
