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

// Independent threadpool and strand (base class strand uses network pool).
chaser_transaction::chaser_transaction(full_node& node) NOEXCEPT
  : chaser(node),
    transaction_threadpool_(one, node.node_settings().pool.thread_priority_()),
    transaction_strand_(transaction_threadpool_.service().get_executor()),
    scan_threadpool_(one, node.node_settings().thread_priority_()),
    revalidation_threadpool_(one, node.node_settings().thread_priority_()),
    outbound_excluded_(unmask_right<uint64_t>(
        node.node_config().manual_relay_slots())),
    inbound_excluded_(unmask_right<uint64_t>(
        node.node_config().manual_relay_slots() +
        node.node_config().outbound_relay_slots()))
{
}

// start
// ----------------------------------------------------------------------------

code chaser_transaction::start() NOEXCEPT
{
    SUBSCRIBE_CHASE(handle_chase, _1, _2);
    POST(do_start);
    return error::success;
}

// Index is rebuilt at open, so only blocks popped since pool top reorganize.
void chaser_transaction::do_start() NOEXCEPT
{
    BC_ASSERT(stranded());
    do_bump();
    if (closed() || !is_indexing())
        return;

    auto& query = archive();
    auto link = query.get_pool_top();
    while (!link.is_terminal() && !query.is_confirmed_block(link))
    {
        if (!unvalidate(link))
        {
            fault(error::transaction10);
            return;
        }

        if (pooling_ && !repool(link))
            return;

        link = query.to_parent(link);
    }

    const auto top = query.to_top_confirmed();
    if (query.get_pool_top() != top && !query.set_pool_top(top))
    {
        fault(error::transaction10);
        return;
    }

    if (!sweep())
        fault(error::transaction11);
}

void chaser_transaction::stopping(const code& ec) NOEXCEPT
{
    stopping_.store(true);
    scan_threadpool_.stop();
    revalidation_threadpool_.stop();
    transaction_threadpool_.stop();
    chaser::stopping(ec);
}

// The strand is no longer running when the chaser is stopped.
void chaser_transaction::stop() NOEXCEPT
{
    if (!scan_threadpool_.join() || !revalidation_threadpool_.join() ||
        !transaction_threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }

    subscriber_.stop_default(network::error::service_stopped);
}

// pool subscription
// ----------------------------------------------------------------------------

void chaser_transaction::subscribe_pool(pool_notifier&& handler) NOEXCEPT
{
    POST(do_subscribe_pool, std::move(handler));
}

void chaser_transaction::do_subscribe_pool(
    const pool_notifier& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    subscriber_.subscribe(pool_notifier{ handler });
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
        {
            POST(do_organized, to_payload<chase::organized>(value).link);
            break;
        }
        case chase::block:
        {
            POST(do_block, to_payload<chase::block>(value).link);
            break;
        }
        case chase::reorganized:
        {
            POST(do_reorganized, to_payload<chase::reorganized>(value).link);
            break;
        }
        case chase::pruned:
        {
            POST(do_pruned, to_payload<chase::pruned>(value).link);
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

// Pool is closed until confirmed chain is current, and closes again if
// currency is lost, though store latch is one way, since txs archived while
// it was open outlive it.
void chaser_transaction::do_bump() NOEXCEPT
{
    BC_ASSERT(stranded());
    estimating_ = false;
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
        const auto top = top_.is_terminal() ? query.to_top_confirmed() : top_;
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
        const auto previous = pool_;
        pool_ = chain_state{ *state, system_settings() }.context();
        previous_ = state->hash();

        // Row holds unless a flag changes or height or median time past falls.
        due_ = due_ || pool_.flags != previous.flags ||
            pool_.height < previous.height ||
            pool_.median_time_past < previous.median_time_past;

        if (!sweep())
        {
            fault(error::transaction11);
            return;
        }

        if (reusable(pool_))
        {
            query.set_pooling();
            pooling_ = true;

            // Rows that arrived while the pool was closed are not in it.
            if (!opened)
                floor_ = std::max(floor_, pool_.height);

            // Make sweeps deferred to block.
            swept_ = {};
            if (!sweeping_)
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
    if (state_ && height == state_->height() &&
        query.get_header_key(top) == state_->hash())
        return state_;

    if (state_ && height == add1(state_->height()))
    {
        const auto header = query.get_header(top);
        if (header && header->previous_block_hash() == state_->hash())
        {
            state_ = to_shared<chain_state>(*state_, *header, system_settings());
            return state_;
        }
    }

    state_ = query.get_confirmed_chain_state(system_settings(), top, height);
    return state_;
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
    return archive().is_sufficient(database::context::from(pool_),
        database::context::from(ctx));
}

// Sweep remains due while it leaves any row unsatisfied, so row is restored
// once pool context satisfies it.
bool chaser_transaction::sweep() NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();
    if (!due_ || !query.is_pooling())
        return true;

    size_t count{};
    const auto start = now();
    const auto applied = query.set_pool_context(count,
        database::context::from(pool_));

    report(pool_scan::context, start);
    due_ = !applied || is_nonzero(count);
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
    if (revalidating_)
        return;

    const auto flagged = pool_.flags != retried_.flags;
    const auto moved = unvalidated_ || pool_.height != retried_.height ||
        pool_.median_time_past != retried_.median_time_past;

    if (!flagged && !moved)
        return;

    auto& query = archive();
    database::tx_links links{};
    database::tx_links unvalidated{};
    const auto ctx = database::context::from(pool_);
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

    retried_ = pool_;
    unvalidated_ = false;
    if (links.empty())
        return;

    revalidating_ = true;
    network::io::post(revalidation_threadpool_.service(),
        BIND(do_revalidate, std::move(links), pool_));
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
    revalidating_ = false;
    if (closed())
        return;

    if (unvalidated_ || !is_sufficient(ctx))
    {
        retried_.flags = {};
        unvalidated_ = true;
        due_ = true;
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

    due_ = true;
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

// Index transitions are applied in order confirmed, once store is pooling
// and while relay is enabled, whether or not pool is open.
void chaser_transaction::do_organized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    auto& query = archive();
    if (is_indexing())
    {
        const auto start = now();
        const auto applied = query.set_pool_block(link, true);
        report(pool_scan::block_in, start);
        if (!applied)
        {
            fault(error::transaction10);
            return;
        }

        fire(events::pool_rows, query.pool_records());
    }

    top_ = link;
    ++generation_;
    do_bump();
}

// Block organized before announced, so its transition has been applied.
void chaser_transaction::do_block(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    notify(error::success, chases::block_settled{ link });
}

void chaser_transaction::do_reorganized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    auto& query = archive();
    if (is_indexing())
    {
        const auto start = now();
        const auto applied = query.set_pool_block(link, false) &&
            unvalidate(link);

        report(pool_scan::block_out, start);
        if (!applied)
        {
            fault(error::transaction10);
            return;
        }
    }

    top_ = query.to_parent(link);
    do_bump();
    if (pooling_ && repool(link) && !sweep())
        fault(error::transaction11);
}

// Block's parent becomes pool top with spenders set.
bool chaser_transaction::unvalidate(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    size_t count{};
    if (!archive().set_pool_unvalidated(count, link))
        return false;

    unvalidated_ = unvalidated_ || is_nonzero(count);
    due_ = due_ || is_nonzero(count);
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

// Rows confirmed by the pruned block are not retained by the prune.
void chaser_transaction::do_pruned(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    size_t height{};
    if (!archive().get_height(height, link))
    {
        fault(error::transaction9);
        return;
    }

    floor_ = std::max(floor_, add1(height));
    estimating_ = false;
}

// utility
// ----------------------------------------------------------------------------

chaser_transaction::time_point chaser_transaction::now() NOEXCEPT
{
    return std::chrono::steady_clock::now();
}

// Fee and size are recomputed here, as for package rate and block fees, so
// rate could instead be cached on transaction.
size_t chaser_transaction::to_rate(const chain::transaction& tx) NOEXCEPT
{
    const auto size = tx.virtual_size();
    if (is_zero(size))
        return zero;

    // Satoshis per virtual kilobyte, as configured and as advertised (bip133).
    const auto rate = ceilinged_multiply(tx.fee(), 1'000_u64);
    return limit<size_t>(floored_divide(rate, size));
}

// A scan of the frontier form is one pass, of the table's rows.
void chaser_transaction::report(pool_scan scan,
    const time_point& start) NOEXCEPT
{
    BC_ASSERT(stranded());
    switch (scan)
    {
        case pool_scan::admission:
            fire_span(events::pool_admit_usecs, start);
            break;
        case pool_scan::block_in:
            fire_span(events::pool_block_in_usecs, start);
            break;
        case pool_scan::block_out:
            fire_span(events::pool_block_out_usecs, start);
            break;
        case pool_scan::context:
            fire_span(events::pool_context_usecs, start);
            break;
        case pool_scan::template_:
            fire_span(events::pool_template_usecs, start);
            break;
        case pool_scan::projection:
            fire_span(events::pool_project_usecs, start);
            break;
        case pool_scan::confirmable:
            fire_span(events::pool_confirmable_usecs, start);
            break;
        default:
            break;
    }

    if (subscriber_.empty())
        return;

    const auto time = now() - start;
    const auto rows = archive().pool_records();
    subscriber_.notify(error::success, { scan, { rows, one, time } });
}

void chaser_transaction::fire_span(events event,
    const time_point& start) NOEXCEPT
{
    using namespace std::chrono;
    const auto elapsed = now() - start;
    const auto count = duration_cast<microseconds>(elapsed).count();
    fire(event, limit<size_t>(count));
}

// The stored instance of the tx with matching witness, or terminal.
database::tx_link chaser_transaction::to_stored(
    const chain::transaction& tx) const NOEXCEPT
{
    const auto& query = archive();
    for (const auto& link: query.to_duplicates(tx.get_hash(false)))
        if (query.is_witness_match(link, tx))
            return link;

    return {};
}

void chaser_transaction::broadcast(const chain::transaction& tx) const NOEXCEPT
{
    if (!wire_tx_demanded())
        return;

    const auto view = emplace_shared<const chain::view::transaction>(
        tx.to_data(true), false, true);
    broadcast(emplace_shared<const network::wire_tx>(view));
}

// Block less header, largest tx count, declared coinbase weight and sigops.
pool::algorithm::totals_t chaser_transaction::block_limit() const NOEXCEPT
{
    constexpr auto count = variable_size(max_block_size / min_transaction_size);
    constexpr auto bytes = chain::header::serialized_size() + count;
    constexpr auto fixed = light_weight_factor * bytes;
    const auto& settings = node_settings().pool;
    const auto reserved = ceilinged_add(fixed, settings.coinbase_weight);
    const auto weight = floored_subtract(max_block_weight, reserved);
    const auto bip141 = pool_.is_enabled(flags::bip141_rule);
    const auto limit = bip141 ? max_fast_sigops : max_block_sigops;
    const auto sigops = floored_subtract(limit, settings.coinbase_sigops);
    return
    {
        {},
        possible_narrow_cast<uint32_t>(weight),
        possible_narrow_cast<uint32_t>(sigops)
    };
}

// Overrides due to independent priority thread pool
// ----------------------------------------------------------------------------

network::io::strand& chaser_transaction::strand() NOEXCEPT
{
    return transaction_strand_;
}

bool chaser_transaction::stranded() const NOEXCEPT
{
    return transaction_strand_.running_in_this_thread();
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
