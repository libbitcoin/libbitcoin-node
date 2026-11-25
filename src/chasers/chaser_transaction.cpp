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

// Independent threadpool and strand (base class strand uses network pool),
// with validation at network priority so that pool work precedes it.
chaser_transaction::chaser_transaction(full_node& node) NOEXCEPT
  : chaser(node),
    transaction_threadpool_(one, node.node_settings().pool.thread_priority_()),
    validation_threadpool_(node.node_settings().threads_()),
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
    validation_threadpool_.stop();
    transaction_threadpool_.stop();
    chaser::stopping(ec);
}

// The strand is no longer running when the chaser is stopped.
void chaser_transaction::stop() NOEXCEPT
{
    if (!scan_threadpool_.join() || !revalidation_threadpool_.join() ||
        !validation_threadpool_.join() || !transaction_threadpool_.join())
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

    context_.top = link;
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

    context_.top = query.to_parent(link);
    do_bump();
    if (pooling_ && repool(link) && !sweep())
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

    context_.floor = std::max(context_.floor, add1(height));
    estimate_.estimating = false;
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
    const auto bip141 = context_.pool.is_enabled(flags::bip141_rule);
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
