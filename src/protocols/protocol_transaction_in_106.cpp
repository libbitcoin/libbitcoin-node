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

// The set of a requested tx whose set has been submitted.
constexpr size_t late = max_size_t;

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

void protocol_transaction_in_106::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!requested_.empty())
        fire(events::tx_unserved, requested_.size());

    request_timer_->stop();
    protocol_peer::stopping(ec);
}

// Identity.
// ----------------------------------------------------------------------------

type_id protocol_transaction_in_106::inventory_type() const NOEXCEPT
{
    return type_id::transaction;
}

type_id protocol_transaction_in_106::get_data_type() const NOEXCEPT
{
    return tx_type_;
}

hash_digest protocol_transaction_in_106::tx_identifier(
    const chain::transaction& tx) const NOEXCEPT
{
    return tx.get_hash(false);
}

database::tx_link protocol_transaction_in_106::to_known(
    const hash_digest& hash) const NOEXCEPT
{
    const auto& query = archive();
    const auto link = query.to_tx(hash);
    return !link.is_terminal() &&
        (query.is_pooled(link) || query.is_confirmed_tx(link)) ? link :
        database::tx_link{};
}

// Inbound (inv).
// ----------------------------------------------------------------------------

bool protocol_transaction_in_106::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Ignore non-tx inventory.
    if (is_zero(message->count(inventory_type())))
        return true;

    // Relay is implied by protocol attachment, so peer is not in violation.
    if (!is_current_chain(true))
        return true;

    // Peer told not to announce is not asked for its announcements.
    if (silenced())
        return true;

    const auto idle = order_.empty();
    const auto getter = create_get_data(*message);
    if (getter.items.empty())
        return true;

    if (requested_.size() > maximum_requests_)
    {
        LOGR("Excess tx requests (" << requested_.size() << ") of ["
            << opposite() << "].");
        stop(error::stalled_channel);
        return false;
    }

    if (idle)
    {
        oldest_ = network::steady_clock::now();
        start_request_timer(request_timer_->timeout());
    }

    LOGP("Requested (" << getter.items.size() << ") txs from ["
        << opposite() << "].");

    SEND(getter, handle_send, _1);
    return true;
}

// private
// Txs requested of announcement are one set, in its order.
get_data protocol_transaction_in_106::create_get_data(
    const inventory& message) NOEXCEPT
{
    // bip144: get_data uses witness type_id but inv does not.

    get_data getter{};
    announcement set{};
    database::tx_links known{};
    const auto sequence = first_ + sets_.size();
    getter.items.reserve(message.count(inventory_type()));
    for (const auto& item: message.view(inventory_type()))
    {
        // The peer has the tx, so it is not announced back to it.
        const auto repeated = was_announced(item.hash);
        set_announced(item.hash);

        // Known tx is held by peer (pooled marked once), tx requested once.
        const request slot{ sequence, set.hashes.size(), next_ };
        if (const auto link = to_known(item.hash); !link.is_terminal())
        {
            if (!repeated && archive().is_pooled(link))
                known.push_back(link);

            continue;
        }

        if (!requested_.emplace(item.hash, slot).second)
            continue;

        order_.push_back({ item.hash, next_++ });
        getter.items.emplace_back(get_data_type(), item.hash);
        set.hashes.push_back(item.hash);
    }

    if (!set.hashes.empty())
    {
        set.open = set.hashes.size();
        set.txs.resize(set.open);
        sets_.push_back(std::move(set));
    }

    if (!known.empty())
        set_relayed(known);

    getter.items.shrink_to_fit();
    return getter;
}

// request timeout
// ----------------------------------------------------------------------------

// private
bool protocol_transaction_in_106::is_answered(
    const pending& entry) const NOEXCEPT
{
    const auto it = requested_.find(entry.hash);
    return it == requested_.end() || it->second.id != entry.id;
}

// private
void protocol_transaction_in_106::settle() NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto size = order_.size();
    while (!order_.empty() && is_answered(order_.front()))
        order_.pop_front();

    if (order_.size() != size)
        oldest_ = network::steady_clock::now();
}

// private
void protocol_transaction_in_106::start_request_timer(
    const duration& span) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (is_nonzero(request_timer_->timeout().count()))
        request_timer_->start(BIND(handle_request_timer, _1), span);
}

// private
// Timer is not reset by an answer, so expiry is tested against oldest.
void protocol_transaction_in_106::handle_request_timer(
    const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || ec == network::error::operation_canceled ||
        ec == network::error::service_stopped)
        return;

    if (ec)
    {
        LOGF("Tx request timer failure, " << ec.message());
        stop(ec);
        return;
    }

    if (order_.empty())
        return;

    const auto timeout = request_timer_->timeout();
    const auto elapsed = network::steady_clock::now() - oldest_;
    if (elapsed < timeout)
    {
        start_request_timer(timeout - elapsed);
        return;
    }

    LOGR("Tx request timeout of [" << opposite() << "].");
    stop(error::stalled_channel);
}

// accept transaction
// ----------------------------------------------------------------------------

// private
void protocol_transaction_in_106::advance(size_t set) NOEXCEPT
{
    BC_ASSERT(stranded());

    while (first_ < set)
        flush();
}

// private
// Tx of set yet to arrive is a set of its own when it does.
void protocol_transaction_in_106::flush() NOEXCEPT
{
    BC_ASSERT(stranded());

    auto set = std::move(sets_.front());
    sets_.pop_front();
    ++first_;

    chain::transaction_cptrs txs{};
    for (size_t slot{}; slot < set.txs.size(); ++slot)
    {
        if (set.txs.at(slot))
        {
            txs.push_back(set.txs.at(slot));
            continue;
        }

        const auto it = requested_.find(set.hashes.at(slot));
        if (it != requested_.end())
        {
            it->second.set = late;
            set.whole = false;
        }
    }

    if (txs.empty())
        return;

    const auto count = txs.size();
    submit_announced(to_shared(std::move(txs)), relay_slot(),
        BIND(handle_submit, _1, _2, count, set.whole, gate()));
}

// protected
bool protocol_transaction_in_106::set_not_found(
    const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto it = requested_.find(hash);
    if (it == requested_.end())
        return false;

    const auto at = it->second;
    requested_.erase(it);
    settle();
    if (at.set == late)
        return true;

    advance(at.set);
    auto& set = sets_.front();
    set.whole = false;
    if (is_zero(--set.open))
        flush();

    return true;
}

bool protocol_transaction_in_106::handle_receive_transaction(const code& ec,
    const transaction::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& tx = message->transaction_ptr;
    const auto it = requested_.find(tx_identifier(*tx));
    if (it == requested_.end())
    {
        LOGR("Unrequested tx [" << encode_hash(tx_identifier(*tx))
            << "] from [" << opposite() << "].");
        stop(network::error::protocol_violation);
        return false;
    }

    const auto at = it->second;
    requested_.erase(it);
    settle();
    if (at.set == late)
    {
        submit_announced(to_shared(chain::transaction_cptrs{ tx }),
            relay_slot(), BIND(handle_submit, _1, _2, one, false, gate()));

        return true;
    }

    advance(at.set);
    auto& set = sets_.front();
    set.txs.at(at.slot) = tx;
    set.weight = ceilinged_add(set.weight, tx->weight());

    // A set is submitted when none of it is yet to arrive, or it holds a block.
    if (is_zero(--set.open) || set.weight >= chain::max_block_weight)
        flush();

    return true;
}

// protected
void protocol_transaction_in_106::handle_submit(const code& ec, size_t,
    size_t count, bool whole, const gate_t::ptr& gate) NOEXCEPT
{
    POST(do_handle_submit, ec, count, whole, gate);
}

// protected
void protocol_transaction_in_106::do_handle_submit(const code& ec,
    size_t count, bool, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    // Conflict with an unconfirmed tx is not misbehavior, as peer cannot know
    // what is held here, and a requested tx may be archived from another peer
    // before it arrives. Unknown prevout is not misbehavior, as announcement
    // order is not constrained and orphans are not pooled. Locked tx is final
    // to a peer one block ahead, as locks are evaluated against presumed next
    // block, so neither lock is misbehavior. Rate and suspension cannot be
    // advertised below bip133, so a tx violating either is not misbehavior
    // (see protocol_transaction_in_70013). Peer cannot know extent of a
    // conflict component held here.
    if (ec &&
        (ec != error::pooling_disabled) &&
        (ec != error::insufficient_fee) &&
        (ec != error::duplicate_transaction) &&
        (ec != database::error::pool_limit) &&
        (ec != system::error::double_spend) &&
        (ec != system::error::absolute_time_locked) &&
        (ec != system::error::relative_time_locked) &&
        (ec != system::error::missing_previous_output))
    {
        LOGR("Tx from [" << opposite() << "] " << ec.message());
        fire(events::tx_dropped, count);
        stop(ec);
        return;
    }

    if (ec)
    {
        LOGP("Refused (" << count << ") txs from [" << opposite() << "] "
            << ec.message());
        fire(events::tx_refused, count);
    }
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
