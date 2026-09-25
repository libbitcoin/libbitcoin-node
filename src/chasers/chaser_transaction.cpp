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
#include <bitcoin/node/chasers/chaser_transaction.hpp>

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_transaction

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

chaser_transaction::chaser_transaction(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

code chaser_transaction::start() NOEXCEPT
{
    SUBSCRIBE_CHASE(handle_chase, _1, _2);
    POST(do_bump);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_transaction::handle_chase(const code&, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (to_chase(value))
    {
        case chase::organized:
        case chase::reorganized:
        {
            POST(do_bump);
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// The pool is closed until the confirmed chain is current, and closes again if
// currency is lost, though the store latch is one way, since the txs archived
// while it was open outlive it.
void chaser_transaction::do_bump() NOEXCEPT
{
    BC_ASSERT(stranded());
    pooling_ = false;

    // Pooling is a permanent store property, so it requires relay.
    if (closed() || !network_settings().enable_relay || !is_current_chain(true))
        return;

    auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto state = query.get_confirmed_chain_state(system_settings(),
        query.to_confirmed(top), top);

    if (!state)
    {
        fault(error::transaction1);
        return;
    }

    // The context of the next block, in which a pool tx would confirm.
    pool_ = chain_state{ *state, system_settings() }.context();
    query.set_pooling();
    pooling_ = true;
}

// methods
// ----------------------------------------------------------------------------

void chaser_transaction::submit(const transactions_cptr& txs, bool test,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, test, std::move(handler));
}

// private
void chaser_transaction::do_submit(const transactions_cptr& txs, bool test,
    const submit_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, {});
        return;
    }

    size_t index{};
    if (const auto ec = validate(index, *txs))
    {
        handler(ec, index);
        return;
    }

    if (test)
    {
        handler(error::success, {});
        return;
    }

    auto& query = archive();
    database::tx_links fresh(txs->size(), database::tx_link::terminal);
    for (index = zero; index < txs->size(); ++index)
    {
        bool pooled{};
        database::tx_link link{};
        const auto& tx = *txs->at(index);

        // Disk full may leave package partly archived, resolves by resubmit.
        if (const auto ec = query.set_code(link, pooled, tx))
        {
            handler(fault(ec), index);
            return;
        }

        if (!pooled)
            fresh.at(index) = link;

        fire(events::tx_archived, to_rate(tx));
        notify(error::success, chases::transaction{ link });
    }

    // Package parents are resolved by hash, so the whole package precedes.
    for (index = zero; index < txs->size(); ++index)
    {
        const database::tx_link link{ fresh.at(index) };
        if (!link.is_terminal() &&
            !query.set_tx_state(link, *txs->at(index), pool_))
        {
            handler(fault(error::transaction2), index);
            return;
        }
    }

    handler(error::success, {});
}

// utility
// ----------------------------------------------------------------------------

// The fee and size are recomputed here, as they are for the package rate and
// for block fees, so the rate could instead be cached on the transaction.
size_t chaser_transaction::to_rate(const chain::transaction& tx) NOEXCEPT
{
    const auto size = tx.virtual_size();
    if (is_zero(size))
        return zero;

    // Satoshis per virtual kilobyte, as configured and as advertised (bip133).
    const auto rate = ceilinged_multiply(tx.fee(), 1'000_u64);
    return limit<size_t>(floored_divide(rate, size));
}

// validation
// ----------------------------------------------------------------------------

code chaser_transaction::validate(size_t& index,
    const transaction_cptrs& txs) NOEXCEPT
{
    if (txs.empty())
        return error::empty_package;

    if (block::is_internal_double_spend(txs, false))
        return system::error::block_internal_double_spend;

    if (const auto ec = block::populate(txs, pool_, false))
        return ec;

    for(const auto& tx: txs)
        if (const auto ec = validate(*tx))
            return ec;


    uint64_t fee{}, size{};
    for (const auto& tx: txs)
    {
        fee = ceilinged_add(fee, tx->fee());
        size = ceilinged_add(size, possible_wide_cast<uint64_t>(
            tx->virtual_size()));
    }

    // Compared in satoshis per virtual kilobyte.
    const auto rate = node_settings().minimum_fee_rate_();
    return ceilinged_multiply(fee, 1'000_u64) < ceilinged_multiply(rate, size) ?
        error::insufficient_fee : error::success;
}

code chaser_transaction::validate(const chain::transaction& tx) NOEXCEPT
{
    code ec{};

    // Ensure tx does not violate tx consensus rules.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(pool_);
    if (!ec) archive().populate_with_metadata(tx, true, true);
    if (!ec) ec = tx.accept(pool_);

    // Ensure tx does not violate presumed block consensus rules.
    // This is a DoS guard when validating a tx outside of a block.
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(pool_);
    if (!ec) ec = tx.accept_guard(pool_);
    if (!ec) ec = tx.confirm_guard(pool_);

    // Script validation is the most costly, so it follows the guards.
    if (!ec) ec = tx.connect(pool_);
    return ec;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
