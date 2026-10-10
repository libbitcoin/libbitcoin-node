/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_106

using namespace system;
using namespace network::messages::peer;
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
    subscribe_chase(BIND(handle_chase, _1, _2));

    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    protocol_peer::start();
}

void protocol_transaction_out_106::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Unsubscriber race is ok.
    unsubscribe_chase();
    protocol_peer::stopping(ec);
}

// Identity.
// ----------------------------------------------------------------------------

type_id protocol_transaction_out_106::inventory_type() const NOEXCEPT
{
    return type_id::transaction;
}

hash_digest protocol_transaction_out_106::tx_identifier(
    transaction_t link) const NOEXCEPT
{
    return archive().get_tx_key(link);
}

database::tx_link protocol_transaction_out_106::to_transaction(
    const inventory_item& item) const NOEXCEPT
{
    return archive().to_tx(item.hash);
}

// handle events (transaction)
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::handle_chase(const code&,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (to_chase(value))
    {
        case chase::transaction:
        {
            POST(do_announce, to_payload<chase::transaction>(value).link);
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

bool protocol_transaction_out_106::do_announce(transaction_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return false;

    return announce(tx_identifier(link));
}

bool protocol_transaction_out_106::announce(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (was_announced(hash))
        return true;

    if (hash == null_hash)
    {
        ////stop(fault(system::error::not_found));
        LOGF("Organized transaction not found.");
        return true;
    }

    // bip144: get_data uses witness type_id but inv does not.
    const inventory inv{ { { inventory_type(), hash } } };
    NOTIFY(inv, handle_send, _1);
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

    send_transaction(error::success, zero, message, gate());
    return true;
}

// Outbound (tx).
// ----------------------------------------------------------------------------

void protocol_transaction_out_106::send_transaction(const code& ec,
    size_t index, const get_data::cptr& message,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    const auto& query = archive();
    chain::transaction::cptr ptr{};
    auto witness = false;

    // Drain unservable items, skipping non-tx inventory. The derived protocol
    // accumulates them if it reports them, and otherwise stops the channel.
    for (; index < message->items.size(); ++index)
    {
        const auto& item = message->items.at(index);
        if (!item.is_transaction_type())
            continue;

        // A witness hash identifies a witness serialization (bip339).
        witness = item.is_witness_type() || item.is_type(type_id::wtxid);
        if (!node_witness_ && witness)
        {
            LOGR("Unsupported witness get_data from [" << opposite() << "].");
            stop(network::error::protocol_violation);
            return;
        }

        // Tx could be always queried with witness and therefore safely cached.
        // If can then be serialized according to channel configuration, however
        // that is currently fixed to witness as available in the object.
        ptr = query.get_transaction(to_transaction(item), witness);
        if (ptr)
            break;

        LOGV("Requested tx " << encode_hash(item.hash)
            << " from [" << opposite() << "] not found.");

        // This tx could not have been advertised to the peer.
        if (!handle_unservable(item))
            return;
    }

    // The report resumes this loop on completion, so it precedes the tx.
    if (report_unservable(index, message, gate))
        return;

    if (index >= message->items.size())
        return;

    SEND(transaction{ ptr }, send_transaction, _1, add1(index), message, gate);
}

// not_found is undefined below bip37, so the channel is stopped instead.
bool protocol_transaction_out_106::handle_unservable(
    const inventory_item& LOG_ONLY(item)) NOEXCEPT
{
    BC_ASSERT(stranded());

    LOGR("Unservable tx " << encode_hash(item.hash) << " from ["
        << opposite() << "], stopping.");

    stop(system::error::not_found);
    return false;
}

// There is nothing to report below bip37, the channel is stopped above.
bool protocol_transaction_out_106::report_unservable(size_t,
    const get_data::cptr&, const gate_t::ptr&) NOEXCEPT
{
    return false;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
