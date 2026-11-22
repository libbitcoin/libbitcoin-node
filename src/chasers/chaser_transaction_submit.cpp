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

// submit
// ----------------------------------------------------------------------------

// The first measured component over a limit, its check, counts and members
// (K candidate, otherwise the pooled wtxid).
static std::string describe_limit(const node::query& query,
    const node::query::pool_graph& graph, size_t given,
    const pool::algorithm::totals_t& limit, size_t maximum_outcomes) NOEXCEPT
{
    using algorithm = pool::algorithm;
    const auto& g = graph.graph;
    const auto rows = g.table.cells.size();
    const auto members = [&](const algorithm::component_t& component)
        NOEXCEPT
    {
        std::ostringstream out{};
        for (size_t at{}; at < component.count; ++at)
        {
            const auto row = component.rows[at];
            if (row >= rows)
            {
                out << " K" << (row - rows);
                continue;
            }

            out << " " << encode_hash(query.get_wtxid_key(
                database::pool_link{ row }));
        }

        return out.str();
    };

    std::ostringstream out{};
    for (auto at = given; at < graph.candidates.size(); ++at)
    {
        algorithm::component_t component{};
        const auto candidate = graph.candidates.at(at);
        if (!algorithm::collect(component, g, candidate))
        {
            out << "candidate " << at << " collect over "
                << algorithm::maximum_members << " members:"
                << members(component);
            return out.str();
        }

        if (!algorithm::relate(component, g))
        {
            out << "candidate " << at << " relate over "
                << algorithm::maximum_members << " contests, members "
                << component.count << ":" << members(component);
            return out.str();
        }

        algorithm::totals_t sum{};
        const auto ancestry = is_zero(component.contests) ||
            algorithm::ancestry(sum, g, component, limit);
        const auto outcomes = algorithm::outcomes(component);
        if (!ancestry || outcomes > maximum_outcomes)
        {
            out << "candidate " << at << (ancestry ? " outcomes " :
                " ancestry ") << "members " << component.count
                << " contests " << component.contests << " outcomes "
                << outcomes << " size " << sum.size << " sigops "
                << sum.sigops << ":" << members(component);
            return out.str();
        }
    }

    return "no component over a limit";
}

void chaser_transaction::submit(const transactions_cptr& txs, bool test,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, test, false, relay_slots, std::move(handler));
}

void chaser_transaction::submit_announced(const transactions_cptr& txs,
    size_t slot, submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, false, true, slot, std::move(handler));
}

// private
// A set is tested prefix by prefix. The running set is its unstored txs since
// the last prefix to clear, and clears when its coverage does.
// It is then connected and admitted, and the next tx starts another. An
// announced set is a batch, not a package, so a tx of it that is not accepted,
// or that exceeds a pool limit, is passed over as if stored. A tx is accepted
// only if each parent is confirmed, pooled, or accepted before it in the set.
void chaser_transaction::do_submit(const transactions_cptr& txs, bool test,
    bool announced, size_t slot, const submit_handler& handler) NOEXCEPT
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

    const auto begin = now();
    submission set{ txs, slot, network::unix_time(), test, announced };
    hold(set);
    if (const auto ec = check(*txs, set.stored, announced))
    {
        handler(ec, {});
        return;
    }

    for (size_t index{}; index < txs->size(); ++index)
    {
        if (set.stored.at(index))
            continue;

        const auto& tx = *txs->at(index);
        auto ec = accept(tx, pool_);
        if (!ec && !spends_held(set, tx))
            ec = system::error::missing_previous_output;

        if (ec)
        {
            if (!announced)
            {
                handler(ec, index);
                return;
            }

            pass_over(set, index, ec);
            continue;
        }

        bool covered{};
        set.held.insert(tx.get_hash(false));
        ec = extend(covered, set, index);
        if (ec == database::error::pool_limit)
        {
            if (!announced)
            {
                handler(ec, set.first);
                return;
            }

            pass_over(set, index, ec);
            ec = restore(set, index);
        }

        if (ec)
        {
            handler(ec, index);
            return;
        }

        if (!covered)
            continue;

        size_t at{};
        ec = clear(at, set, index);
        if (ec)
        {
            handler(ec, at);
            return;
        }
    }

    if (set.cleared)
        fire_span(events::pool_submit_usecs, begin);

    // A set that ends uncleared fails at the first tx of the uncleared set.
    if (set.failure)
        handler(set.failure, set.passed);
    else if (set.running)
        handler(error::insufficient_fee, set.first);
    else
        handler(error::success, {});
}

// private
// A tx of the set is stored if pooled or confirmed (other than a coinbase),
// and if pooled is held by the peer that sent it.
void chaser_transaction::hold(submission& set) NOEXCEPT
{
    const auto& query = archive();
    set.stored.resize(set.txs->size());
    for (size_t index{}; index < set.txs->size(); ++index)
    {
        const auto& tx = *set.txs->at(index);
        const auto link = to_stored(tx);
        set.stored.at(index) = !tx.is_coinbase() && !link.is_terminal() &&
            (query.is_pooled(link) || query.is_confirmed_tx(link));

        if (!set.stored.at(index))
            continue;

        set.held.insert(tx.get_hash(false));
        if (set.slot >= relay_slots)
            continue;

        const auto row = query.to_pool_row(link);
        if (!row.is_terminal())
            set_right_into(marks(row.value), set.slot);
    }
}

// private
bool chaser_transaction::spends_held(const submission& set,
    const chain::transaction& tx) const NOEXCEPT
{
    return std::ranges::all_of(*tx.inputs_ptr(), [&](const auto& in) NOEXCEPT
    {
        return set.held.contains(in->point().hash()) || is_held(*in);
    });
}

// private
bool chaser_transaction::spends_held(
    const chain::transaction& tx) const NOEXCEPT
{
    return std::ranges::all_of(*tx.inputs_ptr(), [&](const auto& in) NOEXCEPT
    {
        return is_held(*in);
    });
}

// private
bool chaser_transaction::is_held(const chain::input& in) const NOEXCEPT
{
    const auto& query = archive();
    const database::tx_link parent{ in.metadata.parent_tx };
    return !parent.is_terminal() &&
        (query.is_confirmed_tx(parent) || query.is_pooled(parent));
}

// private
void chaser_transaction::pass_over(submission& set, size_t index,
    const code& ec) NOEXCEPT
{
    if (!set.failure)
    {
        set.failure = ec;
        set.passed = index;
    }

    set.stored.at(index) = true;
    set.held.erase(set.txs->at(index)->get_hash(false));
}

// private
void chaser_transaction::count(submission& set,
    const chain::transaction& tx) const NOEXCEPT
{
    const auto bip16 = pool_.is_enabled(flags::bip16_rule);
    const auto bip141 = pool_.is_enabled(flags::bip141_rule);
    const auto sigops = tx.signature_operations(bip16, bip141);
    set.bytes = ceilinged_add<uint64_t>(set.bytes, tx.virtual_size());
    set.sigops = ceilinged_add<uint64_t>(set.sigops, sigops);
}

// private
// The running set is bounded by a block, and its coverage by the pool limits.
code chaser_transaction::extend(bool& covered, submission& set,
    size_t index) NOEXCEPT
{
    covered = false;
    auto& query = archive();
    const auto& tx = *set.txs->at(index);
    const auto ctx = database::context::from(pool_);
    if (const auto ec = query.get_pool_candidate(set.graph, tx, ctx))
        return fault(ec);

    if (!set.running)
        set.first = index;

    set.running = true;
    count(set, tx);

    const auto limit = block_limit();
    if (set.bytes > limit.size || set.sigops > limit.sigops)
    {
        LOGN("Pool limit [" << encode_hash(tx.get_hash(false))
            << "] running set over a block");
        return database::error::pool_limit;
    }

    const auto& settings = node_settings().pool;
    const auto start = now();
    const auto ec = query.get_pool_coverage(covered, set.graph, set.given,
        settings.minimum_fee_rate_(), settings.bytes_per_sigop, limit,
        settings.maximum_outcomes);

    report(pool_scan::coverage, start);
    if (ec == database::error::pool_limit)
    {
        LOGN("Pool limit [" << encode_hash(tx.get_hash(false)) << "] "
            << describe_limit(query, set.graph, set.given, limit,
                settings.maximum_outcomes));
        return ec;
    }

    if (ec)
        return fault(ec);

    if (covered)
        fire_span(events::pool_coverage_usecs, start);

    return error::success;
}

// private
// The running set before the index, without any tx passed over.
code chaser_transaction::restore(submission& set, size_t index) NOEXCEPT
{
    auto& query = archive();
    const auto ctx = database::context::from(pool_);
    set.graph = {};
    set.bytes = set.sigops = zero;
    set.running = false;
    for (auto at = set.first; at < index; ++at)
    {
        if (set.stored.at(at))
            continue;

        const auto& tx = *set.txs->at(at);
        if (const auto ec = query.get_pool_candidate(set.graph, tx, ctx))
            return fault(ec);

        count(set, tx);
        set.running = true;
    }

    return error::success;
}

// private
// Script validation is the most costly, so it follows the guards. In a test
// the cleared txs are not pooled, so remain given candidates.
code chaser_transaction::clear(size_t& at, submission& set,
    size_t index) NOEXCEPT
{
    const auto connecting = now();
    for (at = set.first; at <= index; ++at)
    {
        if (set.stored.at(at))
            continue;

        if (const auto ec = set.txs->at(at)->connect(pool_))
            return ec;
    }

    fire_span(events::pool_connect_usecs, connecting);

    // The graph holds accessors, released before the index is written.
    if (set.test)
        set.given = set.graph.candidates.size();
    else
        set.graph = {};

    database::tx_links admitted{};
    for (at = set.first; !set.test && at <= index; ++at)
    {
        if (set.stored.at(at))
            continue;

        database::tx_link link{};
        if (const auto ec = admit(link, *set.txs->at(at), set.time))
            return ec;

        admitted.push_back(link);
        set.stored.at(at) = true;
        set.cleared = true;
    }

    if (!admitted.empty() && !relay(admitted, set.slot, relaying_))
    {
        at = set.first;
        return fault(database::error::integrity);
    }

    set.running = false;
    set.bytes = set.sigops = zero;
    return error::success;
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

code chaser_transaction::admit(database::tx_link& link,
    const chain::transaction& tx, uint32_t time) NOEXCEPT
{
    bool archived{};
    auto& query = archive();

    // Disk full may leave a set partly archived, resolves by resubmit.
    if (const auto ec = query.set_code(link, archived, tx))
        return fault(ec);

    if (!query.is_pooled(link))
    {
        size_t repooled{};
        const auto start = now();
        const auto admitted = query.set_pooled(repooled, link, tx,
            database::context::from(pool_), time);

        report(pool_scan::admission, start);
        if (!admitted)
            return fault(error::transaction2);

        // A spender pooled again keeps its context, which may not suffice,
        // and one not validated for want of this parent may now pass.
        if (is_nonzero(repooled))
        {
            due_ = true;
            unvalidated_ = true;
            if (!sweep())
                return fault(error::transaction11);
        }
    }

    if (!archived && query.silent_enabled() && !query.set_silent(link, tx))
        return fault(error::transaction6);

    fire(events::tx_archived, to_rate(tx));
    notify(error::success, chases::transaction{ link });
    broadcast(tx);
    return error::success;
}

code chaser_transaction::accept(const chain::transaction& tx,
    const chain::context& ctx) NOEXCEPT
{
    code ec{};

    // Ensure tx does not violate tx consensus rules.
    // Guards ensure tx does not violate presumed block consensus rules.
    // These are DoS guards when validating a tx outside of a block.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(ctx);
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(ctx);

    // Store population follows all checks that do not require it.
    if (!ec) archive().populate_with_metadata(tx, true, true);
    if (!ec) ec = tx.accept(ctx);
    if (!ec) ec = tx.accept_guard(ctx);
    if (!ec) ec = tx.confirm_guard(ctx);

    // An unconfirmed conflict is admitted by coverage.
    return ec == system::error::double_spend ? error::success : ec;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
