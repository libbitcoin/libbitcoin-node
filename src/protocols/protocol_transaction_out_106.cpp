/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_transaction_out_106.hpp>

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_106

using namespace system;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_transaction_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));

    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    protocol::start();
}

void protocol_transaction_out_106::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (transaction)
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::transaction:
        {
            // value is organized tx pk.
            BC_ASSERT(std::holds_alternative<transaction_t>(value));
            POST(do_organized, std::get<transaction_t>(value));
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Outbound (inv).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::do_organized(transaction_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (stopped())
        return false;

    ///////////////////////////////////////////////////////////////////////////
    // TODO: don't announce to peer that announced to us.
    // TODO: don't announce to peer that is not current.
    ///////////////////////////////////////////////////////////////////////////

    // bip144: get_data uses witness constant but inventory does not.
    // Otherwise must be type_id::transaction. Query and send as requested.
    // bip339: "After a node has received a wtxidrelay message from a peer,
    // the node MUST use the MSG_WTX inv type when announcing transactions..."
    const inventory inv{ { { type_id::transaction, query.get_tx_key(link) } } };
    SEND(inv, handle_send, _1);
    return true;
}

// Inbound (get_data).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::handle_receive_get_data(const code& ec,
    const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Send and desubscribe.
    send_transaction(error::success, zero, message);
    return false;
}

// Outbound (tx).
// ----------------------------------------------------------------------------
// TODO: bip339: "After a node has received a wtxidrelay message from a peer,
// the node SHOULD use a MSG_WTX getdata message to request any announced
// transactions. A node MAY still request transactions from that peer
// using MSG_TX getdata messages." (derived protocol)

void protocol_transaction_out_106::send_transaction(const code& ec,
    size_t index, const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Skip over non-tx inventory.
    for (; index < message->items.size(); ++index)
        if (message->items.at(index).is_transaction_type())
            break;

    if (index >= message->items.size())
    {
        // Complete, resubscribe to transaction requests.
        SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
        return;
    }

    const auto& item = message->items.at(index);
    const auto witness = item.is_witness_type();
    if (!node_witness_ && witness)
    {
        LOGR("Unsupported witness get_data from [" << authority() << "].");
        stop(network::error::protocol_violation);
        return;
    }

    // TODO: implement witness parameter in block/tx queries.
    const auto& query = archive();
    const auto ptr = query.get_transaction(query.to_tx(item.hash) /*, witness*/);

    if (!ptr)
    {
        LOGR("Requested tx " << encode_hash(item.hash)
            << " from [" << authority() << "] not found.");

        // This tx could not have been advertised to the peer.
        stop(system::error::not_found);
        return;
    }

    SEND(transaction{ ptr }, send_transaction, _1, sub1(index), message);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
