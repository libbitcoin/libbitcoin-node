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

constexpr uint64_t vbytes_per_vkbyte = 1'000;

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
    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    POST(do_bump);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_transaction::handle_chase(const code&, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
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

    if (closed() || !is_current_chain(true))
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
    for (index = {}; index < txs->size(); ++index)
    {
        database::tx_link link{};

        // Disk full may leave package partly archived, resolves by resubmit.
        if (const auto ec = query.set_code(link, *txs->at(index)))
        {
            handler(fault(ec), index);
            return;
        }

        fire(events::tx_archived, link);
        notify(error::success, chase::transaction, transaction_t{ link });
    }

    handler(error::success, {});
}

// validation
// ----------------------------------------------------------------------------

code chaser_transaction::validate(size_t& index,
    const transaction_cptrs& txs) NOEXCEPT
{
    index = zero;
    if (txs.empty())
        return error::empty_package;

    if (const auto ec = block::populate(txs, pool_, false))
        return ec;

    for (; index < txs.size(); ++index)
        if (const auto ec = validate(*txs.at(index)))
            return ec;

    // The package is accepted as a whole, so the whole must pay the rate.
    index = zero;
    uint64_t fee{};
    uint64_t size{};
    for (const auto& tx: txs)
    {
        fee = ceilinged_add(fee, tx->fee());
        size = ceilinged_add(size, possible_wide_cast<uint64_t>(
            tx->virtual_size()));
    }

    // Compared in satoshis per virtual kilobyte, so exact and undivided.
    if (ceilinged_multiply(fee, vbytes_per_vkbyte) <
        ceilinged_multiply(node_settings().minimum_fee_rate_(), size))
        return error::insufficient_fee;

    return {};
}

code chaser_transaction::validate(const chain::transaction& tx) NOEXCEPT
{
    code ec{};

    // Ensure tx does not violate tx consensus rules.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(pool_);
    if (!ec) archive().populate_with_metadata(tx, true);
    if (!ec) ec = tx.accept(pool_);
    if (!ec) ec = tx.confirm(pool_);
    if (!ec) ec = tx.connect(pool_);

    // Ensure tx does not violate presumed block consensus rules.
    // This is a DoS guard when validating a tx outside of a block.
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(pool_);
    if (!ec) ec = tx.accept_guard(pool_);
    return ec;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
