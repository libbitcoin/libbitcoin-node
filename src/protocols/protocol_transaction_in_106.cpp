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
#include <bitcoin/node/protocols/protocol_transaction_in_106.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_in_106

using namespace system;
using namespace network::messages::peer;
using namespace std::placeholders;

// The maximum number of txs requested from a peer and not yet received.
constexpr size_t maximum_backlog = max_inventory;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_transaction_in_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(transaction, handle_receive_transaction, _1, _2);
    SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
    protocol_peer::start();
}

// Inbound (inv).
// ----------------------------------------------------------------------------
// TODO: bip339: "After a node has received a wtxidrelay message from a peer,
// the node SHOULD use a MSG_WTX getdata message to request any announced
// transactions."

bool protocol_transaction_in_106::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Ignore non-tx inventory.
    if (is_zero(message->count(type_id::transaction)))
        return true;

    // Relay is implied by protocol attachment, so peer is not in violation.
    if (!is_current_chain(true))
        return true;

    // An announcement is buffered in full, so the backlog is measured only
    // between messages, and may exceed the maximum by one message.
    if (requested_.size() > maximum_backlog)
    {
        LOGR("Excessive tx backlog (" << requested_.size() << ") from ["
            << opposite() << "].");
        stop(error::excessive_backlog);
        return false;
    }

    const auto getter = create_get_data(*message);
    if (getter.items.empty())
        return true;

    LOGP("Requested (" << getter.items.size() << ") txs from ["
        << opposite() << "].");

    SEND(getter, handle_send, _1);
    return true;
}

// private
get_data protocol_transaction_in_106::create_get_data(
    const inventory& message) NOEXCEPT
{
    // bip144: get_data uses witness type_id but inv does not.

    get_data getter{};
    getter.items.reserve(message.count(type_id::transaction));
    for (const auto& item: message.view(type_id::transaction))
    {
        // The peer has the tx, so it is not announced back to it.
        set_announced(item.hash);

        if (!archive().is_tx(item.hash))
        {
            getter.items.emplace_back(tx_type_, item.hash);
            requested_.insert(item.hash);
        }
    }

    getter.items.shrink_to_fit();
    return getter;
}

// accept transaction
// ----------------------------------------------------------------------------

// protected
bool protocol_transaction_in_106::erase_requested(
    const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());

    return !is_zero(requested_.erase(hash));
}

bool protocol_transaction_in_106::handle_receive_transaction(const code& ec,
    const transaction::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& tx = message->transaction_ptr;
    if (!erase_requested(tx->get_hash(false)))
    {
        LOGR("Unrequested tx [" << encode_hash(tx->get_hash(false))
            << "] from [" << opposite() << "].");
        stop(network::error::protocol_violation);
        return false;
    }

    submit(to_shared(chain::transaction_cptrs{ tx }), false,
        BIND(handle_submit, _1, _2));

    return true;
}

// protected
void protocol_transaction_in_106::handle_submit(const code& ec, size_t) NOEXCEPT
{
    POST(do_handle_submit, ec);
}

// protected
void protocol_transaction_in_106::do_handle_submit(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    // Sending a conflict with a confirmed tx is considered misbehavior.
    if (ec && (ec != system::error::double_spend))
    {
        LOGR("Tx from [" << opposite() << "] " << ec.message());
        stop(ec);
    }
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
