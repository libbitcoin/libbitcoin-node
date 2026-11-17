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

#include <unordered_set>
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
  : chaser(node),
    scan_threadpool_(one, node.node_settings().thread_priority_())
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

void chaser_transaction::stopping(const code& ec) NOEXCEPT
{
    stopping_.store(true);
    scan_threadpool_.stop();
    chaser::stopping(ec);
}

// The strand is no longer running when the chaser is stopped.
void chaser_transaction::stop() NOEXCEPT
{
    if (!scan_threadpool_.join())
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

chaser_transaction::time_point chaser_transaction::now() NOEXCEPT
{
    return std::chrono::steady_clock::now();
}

// A scan of the frontier form is one pass, of the table's rows.
void chaser_transaction::report(pool_scan scan,
    const time_point& start) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (subscriber_.empty())
        return;

    const auto time = now() - start;
    const auto rows = archive().pool_records();
    subscriber_.notify(error::success, { scan, { rows, one, time } });
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

// The pool is closed until the confirmed chain is current, and closes again if
// currency is lost, though the store latch is one way, since the txs archived
// while it was open outlive it.
void chaser_transaction::do_bump() NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto opened = pooling_;
    pooling_ = false;
    if (closed() || !is_current_chain(true))
        return;

    // Pooling is a permanent store property, so it requires relay.
    auto& query = archive();
    if (network_settings().enable_relay)
    {
        size_t height{};
        const auto top = query.to_top_confirmed();
        if (!query.get_height(height, top))
        {
            fault(error::transaction9);
            return;
        }

        const auto state = query.get_confirmed_chain_state(system_settings(),
            top, height);

        if (!state)
        {
            fault(error::transaction1);
            return;
        }

        // The context of the next block, in which a pool tx would confirm.
        const auto previous = pool_;
        pool_ = chain_state{ *state, system_settings() }.context();

        // A row holds while no flag changes and neither height nor median
        // time past falls.
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
        }
    }

    // Scan starts are mapped after the latch, as pooled rows are unordered.
    if (!query.silent_enabled() || scanning_)
        return;

    // A complete map reaches genesis, and keeps up with the top until latched.
    // Rows of a block archived before the latch may precede mapped starts.
    const auto top = query.to_top_confirmed();
    const auto unmapped = query.get_scan_start(top).is_terminal();
    if (query.get_scan_start(query.to_confirmed(zero)).is_terminal())
        start_scan(true);
    else if (unmapped && !query.is_pooling())
        start_scan(false);
    else if (unmapped && query.to_coinbase(top) < query.get_latch_tx())
        start_scan(true);
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

// A sweep remains due while it leaves any row unsatisfied, so that the row is
// restored once the pool context satisfies it.
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
    return applied;
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

// Index transitions are applied in the order confirmed, and once the store is
// pooling whether or not the pool is open.
void chaser_transaction::do_organized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    auto& query = archive();
    const auto start = now();
    const auto applied = !query.is_pooling() ||
        query.set_pool_block(link, true);

    report(pool_scan::block_in, start);
    if (!applied)
    {
        fault(error::transaction10);
        return;
    }

    do_bump();
}

// The block was organized before it was announced, so its transition has
// been applied, and its prevout cache is no longer required by the pool.
void chaser_transaction::do_block(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    notify(error::success, chases::block_settled{ link });
}

// Pooled in block order under the block context, excluding any confirmed (by
// another block).
void chaser_transaction::do_reorganized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    auto& query = archive();
    const auto start = now();
    const auto applied = !query.is_pooling() ||
        query.set_pool_block(link, false);

    report(pool_scan::block_out, start);
    if (!applied)
    {
        fault(error::transaction10);
        return;
    }

    do_bump();
    if (!pooling_)
        return;

    chain::context ctx{};
    const auto txs = query.to_transactions(link);
    if (txs.empty() || !query.get_context(ctx, link))
    {
        fault(error::transaction7);
        return;
    }

    if (!reusable(ctx))
        return;

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
            return;
        }

        // Block txs are already silent indexed.
        const auto admit = now();
        const auto pooled = query.set_pooled(fk, *tx, ctx, ctx.timestamp);
        report(pool_scan::admission, admit);
        if (!pooled)
        {
            fault(error::transaction2);
            return;
        }

        due_ = due_ || !is_sufficient(ctx);
        notify(error::success, chases::transaction{ fk });
        broadcast(*tx);
    }

    if (!sweep())
        fault(error::transaction11);
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
}

// methods
// ----------------------------------------------------------------------------

void chaser_transaction::submit(const transactions_cptr& txs, bool test,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, test, false, std::move(handler));
}

void chaser_transaction::submit_announced(const transactions_cptr& txs,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, false, true, std::move(handler));
}

void chaser_transaction::submit_compact(const transactions_cptr& txs,
    const database::tx_links& links, const database::header_link& link,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit_compact, txs, links, link, std::move(handler));
}

void chaser_transaction::build_template(template_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_build_template, std::move(handler));
}

void chaser_transaction::estimate(size_t target, estimate_mode mode,
    estimate_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_estimate, target, mode, std::move(handler));
}

// private
// A set is tested prefix by prefix. The running set is its unstored txs since
// the last prefix to clear, and clears when its own rate and its coverage do.
// It is then connected and admitted, and the next tx starts another. An
// announced set is a batch, not a package, so a tx of it that is not accepted
// is passed over as if stored. Prevouts are populated from the whole set, so
// a tx spending one of it not held (stored or accepted before it) is not.
void chaser_transaction::do_submit(const transactions_cptr& txs, bool test,
    bool announced, const submit_handler& handler) NOEXCEPT
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

    auto& query = archive();
    std::vector<bool> stored(txs->size());
    std::transform(txs->cbegin(), txs->cend(), stored.begin(),
        [&](const auto& tx) NOEXCEPT
        {
            return !tx->is_coinbase() && query.is_tx(tx->get_hash(false));
        });

    if (const auto ec = check(*txs, stored, announced))
    {
        handler(ec, {});
        return;
    }

    const auto& settings = node_settings().pool;
    const auto bip16 = pool_.is_enabled(flags::bip16_rule);
    const auto bip141 = pool_.is_enabled(flags::bip141_rule);
    const auto bytes_per_sigop = settings.bytes_per_sigop;
    const auto rate = settings.minimum_fee_rate_();
    const auto ctx = database::context::from(pool_);
    const auto time = network::unix_time();
    const auto limit = block_limit();

    // In a test the cleared txs are not pooled, so remain given candidates.
    query::pool_graph graph{};
    uint64_t fee{}, charge{}, bytes{}, sigops{};
    size_t given{}, first{}, passed{};
    auto running = false;
    code failure{};

    std::unordered_set<hash_digest> held{};
    for (size_t index{}; index < txs->size(); ++index)
        if (stored.at(index))
            held.insert(txs->at(index)->get_hash(false));

    const auto spends_held = [&](const chain::transaction& tx) NOEXCEPT
    {
        return std::ranges::all_of(*tx.inputs_ptr(), [&](const auto& in)
            NOEXCEPT
        {
            return in->metadata.parent_tx != max_uint32 ||
                held.contains(in->point().hash());
        });
    };

    for (size_t index{}; index < txs->size(); ++index)
    {
        if (stored.at(index))
            continue;

        const auto& tx = *txs->at(index);
        auto ec = accept(tx);
        if (!ec && !spends_held(tx))
            ec = system::error::missing_previous_output;

        if (ec)
        {
            if (!announced)
            {
                handler(ec, index);
                return;
            }

            if (!failure)
            {
                failure = ec;
                passed = index;
            }

            stored.at(index) = true;
            continue;
        }

        held.insert(tx.get_hash(false));
        if (const auto candidate = query.get_pool_candidate(graph, tx, ctx))
        {
            handler(fault(candidate), index);
            return;
        }

        if (!running)
            first = index;

        running = true;
        const auto ops = tx.signature_operations(bip16, bip141);
        const auto cost = ceilinged_multiply<uint64_t>(ops, bytes_per_sigop);
        fee = ceilinged_add(fee, tx.fee());
        charge = ceilinged_add(charge, greater(tx.virtual_size(), cost));
        bytes = ceilinged_add<uint64_t>(bytes, tx.virtual_size());
        sigops = ceilinged_add<uint64_t>(sigops, ops);

        // The running set is bounded by a block.
        if (bytes > limit.size || sigops > limit.sigops)
        {
            handler(database::error::pool_limit, first);
            return;
        }

        // Compared in satoshis per virtual kilobyte.
        if (ceilinged_multiply(fee, 1'000_u64) <
            ceilinged_multiply(rate, charge))
            continue;

        bool covered{};
        const auto start = now();
        const auto coverage = query.get_pool_coverage(covered, graph, given,
            rate, bytes_per_sigop, limit, settings.maximum_outcomes);

        report(pool_scan::coverage, start);
        if (coverage)
        {
            const auto limited = coverage == database::error::pool_limit;
            handler(limited ? coverage : fault(coverage), first);
            return;
        }

        if (!covered)
            continue;

        // Script validation is the most costly, so it follows the guards.
        for (auto at = first; at <= index; ++at)
        {
            if (stored.at(at))
                continue;

            if (const auto connected = txs->at(at)->connect(pool_))
            {
                handler(connected, at);
                return;
            }
        }

        // The graph holds accessors, released before the index is written.
        if (test)
            given = graph.candidates.size();
        else
            graph = {};

        for (auto at = first; !test && at <= index; ++at)
        {
            if (stored.at(at))
                continue;

            if (const auto admitted = admit(*txs->at(at), time))
            {
                handler(admitted, at);
                return;
            }

            stored.at(at) = true;
        }

        running = false;
        fee = charge = bytes = sigops = zero;
    }

    // A set that ends uncleared fails at the first tx of the uncleared set.
    if (failure)
        handler(failure, passed);
    else if (running)
        handler(error::insufficient_fee, first);
    else
        handler(error::success, {});
}

// private
void chaser_transaction::do_submit_compact(const transactions_cptr& txs,
    const database::tx_links& links, const database::header_link& link,
    const submit_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    auto& query = archive();
    if (query.is_associated(link))
    {
        handler(error::duplicate_block, {});
        return;
    }

    chain::context ctx{};
    if (!query.get_context(ctx, link))
    {
        handler(fault(error::transaction3), {});
        return;
    }

    constexpr auto terminal = database::tx_link::terminal;
    const auto count = std::ranges::count(links, terminal);
    if (possible_narrow_sign_cast<size_t>(count) != txs->size())
    {
        handler(error::transaction5, {});
        return;
    }

    using link_t = database::tx_link::integer;
    const auto filler = [&](link_t& fk, size_t index) NOEXCEPT
    {
        database::tx_link out{};
        if (const auto ec = fill(out, *txs->at(index), ctx))
        {
            const auto faulted = node::error::error_category::contains(ec) ||
                database::error::error_category::contains(ec);
            handler(faulted ? ec : unconfirmable(link, ctx.height, ec), index);
            return false;
        }

        fk = out;
        return true;
    };

    // Txs are an arbitrary subset of the block, so block checks are deferred to
    // validation. Each is archived before the next, so in block order a parent
    // is resolved from the store. The coinbase (never a parent within its
    // block) is archived last, as validation indexes only txs linked above it.
    auto filled = links;
    const auto deferred = filled.front() == terminal;
    auto index = deferred ? one : zero;
    for (auto fk = std::next(filled.begin()); fk != filled.end(); ++fk)
    {
        if (*fk != terminal)
            continue;

        if (!filler(*fk, index))
            return;

        ++index;
    }

    if (deferred && !filler(filled.front(), zero))
        return;

    if (pooling_ && !sweep())
    {
        handler(fault(error::transaction11), {});
        return;
    }

    const auto strong = is_under_checkpoint(ctx.height);
    if (const auto ec = query.set_code(link, filled, strong))
    {
        handler(fault(ec), {});
        return;
    }

    notify(error::success, chases::checked{ ctx.height });
    fire(events::block_archived, ctx.height);
    handler(error::success, {});
}

// private
void chaser_transaction::do_build_template(
    const template_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {}, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, {}, {});
        return;
    }

    // The template is the first block of a projection.
    const auto& settings = node_settings().pool;
    query::pool_projection projection{};
    database::tx_links links{};
    const auto start = now();
    const auto ec = archive().get_pool_template(links, projection,
        block_limit(), settings.maximum_chunk_depth, settings.bytes_per_sigop,
        settings.maximum_outcomes);

    report(pool_scan::template_, start);
    handler(ec, pool_.height, links);
}

// private
void chaser_transaction::do_estimate(size_t target, estimate_mode mode,
    const estimate_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (!network_settings().enable_relay)
    {
        handler(error::estimate_disabled, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::estimate_premature, {});
        return;
    }

    if (mode == estimate_mode::unknown || target >= maximum_estimate_horizon)
    {
        handler(error::estimate_false, {});
        return;
    }

    const auto& query = archive();
    const auto& settings = node_settings().pool;
    const auto count = add1(target);
    const auto limit = block_limit();
    query::pool_projection projection{};
    const auto start = now();
    const auto projected = query.get_pool_projection(projection, count,
        limit, settings.maximum_chunk_depth, settings.bytes_per_sigop,
        settings.maximum_outcomes);

    report(pool_scan::projection, start);
    if (projected)
    {
        handler(fault(projected), {});
        return;
    }

    // Arrivals are whole from the floor to the height of the open interval.
    query::pool_arrivals arrivals{};
    if (mode != estimate_mode::basic)
    {
        const auto whole = floored_subtract(pool_.height, floor_);
        const auto heights = std::min(whole, conservative_estimate_window);
        if (const auto ec = query.get_pool_arrivals(arrivals, pool_.height,
            heights, settings.bytes_per_sigop))
        {
            handler(fault(ec), {});
            return;
        }
    }

    const auto arrive = [&](uint64_t rate) NOEXCEPT
    {
        return arriving(arrivals, rate, mode);
    };

    using algorithm = pool::algorithm;
    const auto space = limit.size;
    const auto rate = algorithm::estimate(projection, count, space, arrive);
    if (rate == max_uint64)
    {
        handler(error::estimate_false, {});
        return;
    }

    handler(error::success, std::max(rate, settings.minimum_fee_rate_()));
}

// utility
// ----------------------------------------------------------------------------

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

// The stored tx, or validated and archived, pooled under the block context.
code chaser_transaction::fill(database::tx_link& out,
    const chain::transaction& tx, const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    out = to_stored(tx);
    if (!out.is_terminal())
    {
        if (tx.is_coinbase() || !query.is_pooling() || !reusable(ctx) ||
            query.is_pooled(out) || query.is_confirmed_tx(out))
            return error::success;

        if (const auto ec = validate(tx, ctx))
            return ec;

        const auto admit = now();
        const auto admitted = query.set_pooled(out, tx, ctx, ctx.timestamp);
        report(pool_scan::admission, admit);
        if (!admitted)
            return fault(error::transaction2);

        if (ctx.height >= query.silent_start_height() &&
            ctx.is_enabled(flags::bip341_rule) && !query.set_silent(out, tx))
            return fault(error::transaction6);

        due_ = due_ || !is_sufficient(ctx);
        notify(error::success, chases::transaction{ out });
        broadcast(tx);
        return error::success;
    }

    if (const auto ec = validate(tx, ctx))
        return ec;

    bool pooled{};

    // Disk full may leave txs partly archived, resolves by resubmit.
    if (const auto ec = query.set_code(out, pooled, tx))
        return fault(ec);

    const auto coinbase = tx.is_coinbase();
    if (!pooled && !coinbase)
    {
        if (reusable(ctx))
        {
            const auto admit = now();
            const auto admitted = query.set_pooled(out, tx, ctx,
                ctx.timestamp);

            report(pool_scan::admission, admit);
            if (!admitted)
                return fault(error::transaction2);

            due_ = due_ || !is_sufficient(ctx);
        }

        if (ctx.height >= query.silent_start_height() &&
            ctx.is_enabled(flags::bip341_rule) && !query.set_silent(out, tx))
            return fault(error::transaction6);
    }

    if (!coinbase)
    {
        fire(events::tx_archived, to_rate(tx));
        notify(error::success, chases::transaction{ out });
        broadcast(tx);
    }

    return error::success;
}

void chaser_transaction::broadcast(const chain::transaction& tx) const NOEXCEPT
{
    if (!wire_tx_demanded())
        return;

    const auto view = emplace_shared<const chain::view::transaction>(
        tx.to_data(true), false, true);
    broadcast(emplace_shared<const network::wire_tx>(view));
}

// An identified block with an invalid tx is invalid.
code chaser_transaction::unconfirmable(const database::header_link& link,
    size_t height, const code& ec) NOEXCEPT
{
    if (!archive().set_block_unconfirmable(link))
        return fault(error::transaction4);

    LOGR("Compact block failed check [" << height << "] " << ec.message());
    notify(error::success, chases::unchecked{ link });
    fire(events::block_unconfirmable, height);
    return ec;
}

// The size arriving in a block at or above the rate.
uint64_t chaser_transaction::arriving(const query::pool_arrivals& arrivals,
    uint64_t rate, estimate_mode mode) NOEXCEPT
{
    using algorithm = pool::algorithm;
    uint64_t size{};

    switch (mode)
    {
        case estimate_mode::geometric:
        {
            for (const auto& takes: std::views::reverse(arrivals))
            {
                const auto above = algorithm::above(takes, rate);
                size = to_half(ceilinged_add(size, above));
            }

            return size;
        }
        case estimate_mode::economical:
        {
            const auto window = economical_estimate_window;
            const auto count = std::min(arrivals.size(), window);
            for (size_t age{}; age < count; ++age)
            {
                const auto above = algorithm::above(arrivals.at(age), rate);
                size = ceilinged_add(size, above);
            }

            return is_zero(count) ? size : floored_divide(size, count);
        }
        case estimate_mode::conservative:
        {
            for (const auto& takes: arrivals)
                size = std::max(size, algorithm::above(takes, rate));

            return size;
        }
        default:
        {
            return size;
        }
    }
}

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

// A block less its header, largest tx count and declared coinbase weight.
pool::algorithm::totals_t chaser_transaction::block_limit() const NOEXCEPT
{
    constexpr auto count = variable_size(max_block_size / min_transaction_size);
    constexpr auto bytes = chain::header::serialized_size() + count;
    constexpr auto fixed = light_weight_factor * bytes;
    const auto& settings = node_settings().pool;
    const auto reserved = ceilinged_add(fixed, settings.coinbase_weight);
    const auto weight = floored_subtract(max_block_weight, reserved);
    const auto bip141 = pool_.is_enabled(flags::bip141_rule);
    const auto sigops = bip141 ? max_fast_sigops : max_block_sigops;
    return
    {
        {},
        possible_narrow_cast<uint32_t>(weight / light_weight_factor),
        possible_narrow_cast<uint32_t>(sigops)
    };
}

// validation
// ----------------------------------------------------------------------------

// A tx of an announced set that precedes its parent is not accepted, and
// its txs that spend one point are rivals.
code chaser_transaction::check(const transaction_cptrs& txs,
    const std::vector<bool>& stored, bool announced) NOEXCEPT
{
    if (txs.empty())
        return error::empty_package;

    if (std::ranges::all_of(stored, std::identity{}))
        return error::duplicate_transaction;

    if (!announced && block::is_forward_reference(txs, false))
        return system::error::forward_reference;

    if (!announced && block::is_internal_double_spend(txs, false))
        return system::error::block_internal_double_spend;

    return block::populate(txs, pool_, false);
}

code chaser_transaction::admit(const chain::transaction& tx,
    uint32_t time) NOEXCEPT
{
    bool pooled{};
    database::tx_link link{};
    auto& query = archive();

    // Disk full may leave a set partly archived, resolves by resubmit.
    if (const auto ec = query.set_code(link, pooled, tx))
        return fault(ec);

    if (!pooled)
    {
        const auto start = now();
        const auto admitted = query.set_pooled(link, tx, pool_, time);
        report(pool_scan::admission, start);
        if (!admitted)
            return fault(error::transaction2);

        if (query.silent_enabled() && !query.set_silent(link, tx))
            return fault(error::transaction6);
    }

    fire(events::tx_archived, to_rate(tx));
    notify(error::success, chases::transaction{ link });
    broadcast(tx);
    return error::success;
}

code chaser_transaction::accept(const chain::transaction& tx) NOEXCEPT
{
    code ec{};

    // Ensure tx does not violate tx consensus rules.
    // Guards ensure tx does not violate presumed block consensus rules.
    // These are DoS guards when validating a tx outside of a block.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(pool_);
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(pool_);

    // Store population follows all checks that do not require it.
    if (!ec) archive().populate_with_metadata(tx, true, true);
    if (!ec) ec = tx.accept(pool_);
    if (!ec) ec = tx.accept_guard(pool_);
    if (!ec) ec = tx.confirm_guard(pool_);

    // An unconfirmed conflict is admitted by coverage.
    return ec == system::error::double_spend ? error::success : ec;
}

// A block tx is under proof of work, so it is not subject to policy (fee
// rate, guards) and may conflict with unconfirmed txs.
code chaser_transaction::validate(const chain::transaction& tx,
    const chain::context& ctx) NOEXCEPT
{
    code ec{};
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(ctx);
    if (ec || tx.is_coinbase())
        return ec;

    archive().populate_with_metadata(tx);
    ec = tx.accept(ctx);
    if (!ec) ec = tx.connect(ctx);
    return ec;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
