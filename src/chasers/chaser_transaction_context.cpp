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

// context
// ----------------------------------------------------------------------------

// Pool is closed until confirmed chain is current, and closes again if
// currency is lost, though store latch is one way, since txs archived while
// it was open outlive it.
void chaser_transaction::do_bump() NOEXCEPT
{
    BC_ASSERT(stranded());
    estimate_.estimating = false;
    const auto opened = pooling_.load();
    pooling_ = false;
    if (closed() || !is_current_chain(true))
    {
        if (opened)
            ++generation_;

        return;
    }

    // Pooling is a permanent store property, so it requires relay.
    auto& query = archive();
    if (network_settings().enable_relay)
    {
        // Context of block last applied to index (store top may be above).
        size_t height{};
        const auto top = context_.top.is_terminal() ?
            query.to_top_confirmed() : context_.top;
        if (!query.get_height(height, top))
        {
            fault(error::transaction9);
            return;
        }

        const auto state = top_state(top, height);
        if (!state)
        {
            fault(error::transaction1);
            return;
        }

        // The context of the next block, in which a pool tx would confirm.
        const auto previous = context_.pool;
        context_.pool = chain_state{ *state, system_settings() }.context();
        context_.previous = state->hash();

        // Row holds unless a flag changes or height or median time past falls.
        context_.due = context_.due || context_.pool.flags != previous.flags ||
            context_.pool.height < previous.height ||
            context_.pool.median_time_past < previous.median_time_past;

        if (!sweep())
        {
            fault(error::transaction11);
            return;
        }

        if (reusable(context_.pool))
        {
            query.set_pooling();
            pooling_ = true;

            // Rows that arrived while the pool was closed are not in it.
            if (!opened)
                context_.floor = std::max(context_.floor, context_.pool.height);

            // Make sweeps deferred to block.
            relay_.swept = {};
            if (!relay_.sweeping)
                do_sweep_relay();
        }
    }

    // Opening or closing the pool changes its templates.
    if (pooling_ != opened)
        ++generation_;

    // Scan starts are mapped after the latch, as pooled rows are unordered.
    if (!query.silent_enabled() || scanning_)
        return;

    // Complete map reaches genesis, and keeps up with top until latched.
    // Rows of a block archived before latch may precede mapped starts.
    const auto top = query.to_top_confirmed();
    const auto unmapped = query.get_scan_start(top).is_terminal();
    if (query.get_scan_start(query.to_confirmed(zero)).is_terminal())
        start_scan(true);
    else if (unmapped && !query.is_pooling())
        start_scan(false);
    else if (unmapped && query.to_coinbase(top) < query.get_latch_tx())
        start_scan(true);
}

// private
chain_state::cptr chaser_transaction::top_state(
    const database::header_link& top, size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    if (context_.state && height == context_.state->height() &&
        query.get_header_key(top) == context_.state->hash())
        return context_.state;

    if (context_.state && height == add1(context_.state->height()))
    {
        const auto header = query.get_header(top);
        if (header && header->previous_block_hash() == context_.state->hash())
        {
            context_.state = to_shared<chain_state>(*context_.state, *header,
                system_settings());
            return context_.state;
        }
    }

    context_.state = query.get_confirmed_chain_state(system_settings(), top,
        height);
    return context_.state;
}

bool chaser_transaction::is_indexing() const NOEXCEPT
{
    return network_settings().enable_relay && archive().is_pooling();
}

bool chaser_transaction::reusable(const chain::context& ctx) const NOEXCEPT
{
    const auto evaluated = database::context::from(ctx);
    return archive().is_sufficient(evaluated, evaluated);
}

bool chaser_transaction::is_sufficient(
    const chain::context& ctx) const NOEXCEPT
{
    return archive().is_sufficient(database::context::from(context_.pool),
        database::context::from(ctx));
}

// Sweep remains due while it leaves any row unsatisfied, so row is restored
// once pool context satisfies it.
bool chaser_transaction::sweep() NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();
    if (!context_.due || !query.is_pooling())
        return true;

    size_t count{};
    const auto start = now();
    const auto applied = query.set_pool_context(count,
        database::context::from(context_.pool));

    report(pool_scan::context, start);
    context_.due = !applied || is_nonzero(count);
    if (applied && is_nonzero(count))
        start_revalidation();

    return applied;
}

// Rows left out by flags are revalidated once per change of flags, and rows
// not validated once per change of pool context, so failing row is retried
// only under another context.
void chaser_transaction::start_revalidation() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (context_.revalidating)
        return;

    const auto flagged = context_.pool.flags != context_.retried.flags;
    const auto moved = context_.unvalidated ||
        context_.pool.height != context_.retried.height ||
        context_.pool.median_time_past != context_.retried.median_time_past;

    if (!flagged && !moved)
        return;

    auto& query = archive();
    database::tx_links links{};
    database::tx_links unvalidated{};
    const auto ctx = database::context::from(context_.pool);
    code ec{};
    if (flagged)
        ec = query.get_pool_revalidation(links, ctx);

    if (!ec)
        ec = query.get_pool_unvalidated(unvalidated);

    if (ec)
    {
        fault(ec);
        return;
    }

    links.insert(links.end(), unvalidated.begin(), unvalidated.end());

    context_.retried = context_.pool;
    context_.unvalidated = false;
    if (links.empty())
        return;

    context_.revalidating = true;
    network::io::post(revalidation_threadpool_.service(),
        BIND(do_revalidate, std::move(links), context_.pool));
}

// Revalidation is admission under context, as confirmation of what a row spends
// may differ from when it was pooled. Sigops are counted under flags of
// context, so a passing tx is counted again.
void chaser_transaction::do_revalidate(const database::tx_links& links,
    const chain::context& ctx) NOEXCEPT
{
    const auto& query = archive();
    const auto bip16 = ctx.is_enabled(flags::bip16_rule);
    const auto bip141 = ctx.is_enabled(flags::bip141_rule);
    data_chunk passed(links.size());
    std::vector<size_t> sigops(links.size());
    std::for_each(poolstl::execution::par, links.cbegin(), links.cend(),
        [&](const auto& link) NOEXCEPT
        {
            if (stopping_)
                return;

            const auto offset = std::distance(links.data(), &link);
            const auto at = sign_cast<size_t>(offset);
            const auto tx = query.get_transaction(link, true);
            if (!tx || accept(*tx, ctx) || !spends_held(*tx) ||
                tx->connect(ctx))
                return;

            sigops.at(at) = tx->signature_operations(bip16, bip141);
            passed.at(at) = one;
        });

    POST(do_revalidated, links, std::move(passed), std::move(sigops), ctx);
}

// Pool context may have moved under revalidation, and a reorganization under
// it may have unvalidated a row it passed.
void chaser_transaction::do_revalidated(const database::tx_links& links,
    const data_chunk& passed, const std::vector<size_t>& sigops,
    const chain::context& ctx) NOEXCEPT
{
    BC_ASSERT(stranded());
    context_.revalidating = false;
    if (closed())
        return;

    if (context_.unvalidated || !is_sufficient(ctx))
    {
        context_.retried.flags = {};
        context_.unvalidated = true;
        context_.due = true;
        if (!sweep())
            fault(error::transaction11);

        return;
    }

    database::tx_links valid{};
    std::vector<size_t> counts{};
    for (size_t at{}; at < links.size(); ++at)
    {
        if (is_zero(passed.at(at)))
            continue;

        valid.push_back(links.at(at));
        counts.push_back(sigops.at(at));
    }

    const auto count = valid.size();
    const auto revalidated = database::context::from(ctx);
    if (!archive().set_pooled_contexts(valid, counts, revalidated))
    {
        fault(error::transaction2);
        return;
    }

    LOGN("Revalidated (" << count << ") of (" << links.size()
        << ") pooled txs.");

    context_.due = true;
    if (!sweep())
        fault(error::transaction11);
}

void chaser_transaction::start_scan(bool remap) NOEXCEPT
{
    BC_ASSERT(stranded());
    scanning_ = true;

    const auto& query = archive();
    const auto bound = query.is_pooling() ? query.get_scan_frontier() :
        max_size_t;
    const auto top = query.to_top_confirmed();
    network::io::post(scan_threadpool_.service(),
        BIND(do_scan, top, bound, remap));
}

void chaser_transaction::do_scan(const database::header_link& top,
    size_t bound, bool remap) NOEXCEPT
{
    POST(do_scanned, archive().set_scan_starts(stopping_, top, bound, remap));
}

// Blocks organized during the map are mapped by the bump that follows.
void chaser_transaction::do_scanned(bool mapped) NOEXCEPT
{
    BC_ASSERT(stranded());
    scanning_ = false;
    if (closed())
        return;

    if (!mapped)
    {
        fault(error::transaction8);
        return;
    }

    do_bump();
}

// Block's parent becomes pool top with spenders set.
bool chaser_transaction::unvalidate(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    size_t count{};
    if (!archive().set_pool_unvalidated(count, link))
        return false;

    context_.unvalidated = context_.unvalidated || is_nonzero(count);
    context_.due = context_.due || is_nonzero(count);
    return true;
}

// Pooled in block order under block context,
// excluding any confirmed (by another block).
bool chaser_transaction::repool(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();
    chain::context ctx{};
    const auto txs = query.to_transactions(link);
    if (txs.empty() || !query.get_context(ctx, link))
    {
        fault(error::transaction7);
        return false;
    }

    if (!reusable(ctx))
        return true;

    // The coinbase is never pooled.
    for (auto it = std::next(txs.cbegin()); it != txs.cend(); ++it)
    {
        const database::tx_link fk{ *it };
        if (query.is_pooled(fk) || query.is_confirmed_tx(fk))
            continue;

        const auto tx = query.get_transaction(fk, true);
        if (!tx || !query.populate_with_metadata(*tx))
        {
            fault(error::transaction7);
            return false;
        }

        // Block txs are already silent indexed.
        if (!set_pooled(fk, *tx, ctx))
        {
            fault(error::transaction2);
            return false;
        }

        ++generation_;
        notify(error::success, chases::transaction{ fk });
        broadcast(*tx);
    }

    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
