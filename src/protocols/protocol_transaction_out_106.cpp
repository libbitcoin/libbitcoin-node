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
    if (relay())
        subscribe_events(BIND(handle_event, _1, _2, _3));

    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    protocol::start();
}

void protocol_transaction_out_106::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Unsubscriber race is ok.
    if (relay())
        unsubscribe_events();

    protocol::stopping(ec);
}

// handle events (transaction)
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::relay() const NOEXCEPT
{ 
    return true;
}

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
    // TODO: don't send to peer that sent to us.
    ///////////////////////////////////////////////////////////////////////////

    // bip144: get_data uses witness constant but inventory does not.
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

void protocol_transaction_out_106::send_transaction(const code& ec,
    size_t index, const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (index >= message->items.size())
    {
        // Complete, resubscribe to transaction requests.
        SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
        return;
    }

    const auto& query = archive();

    ///////////////////////////////////////////////////////////////////////////
    // TODO: filter for tx types.
    ///////////////////////////////////////////////////////////////////////////
    const auto& hash = message->items.at(index).hash;

    ///////////////////////////////////////////////////////////////////////////
    // TODO: pass witness flag to allow non-witness objects.
    ///////////////////////////////////////////////////////////////////////////
    const auto tx_ptr = query.get_transaction(query.to_tx(hash));

    if (!tx_ptr)
    {
        LOGR("Requested tx not found " << encode_hash(hash)
            << " from [" << authority() << "].");

        // This tx could not have been advertised to the peer.
        stop(system::error::not_found);
        return;
    }

    SEND(transaction{ tx_ptr }, send_transaction, _1, sub1(index), message);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
