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
#include <bitcoin/node/estimator.hpp>

#include <atomic>
#include <cmath>
#include <ranges>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;

// public
// ----------------------------------------------------------------------------

uint64_t estimator::estimate(size_t target, mode mode) const NOEXCEPT
{
    constexpr size_t large = horizon::large;
    if (target >= large)
        return estimate_failed;

    // Valid results are effectively limited to at least 1 sat/vb.
    // threshold_fee is thread safe but values are affected during update. 
    switch (mode)
    {
        case mode::basic:
        {
            return compute(target, confidence::high);
        }
        case mode::geometric:
        {
            return compute(target, confidence::high, true);
        }
        case mode::economical:
        {
            const auto target1 = to_half(target);
            const auto target2 = std::min(one, target);
            const auto target3 = std::min(large, two * target);
            const auto fee1 = compute(target1, confidence::low);
            const auto fee2 = compute(target2, confidence::mid);
            const auto fee3 = compute(target3, confidence::high);
            return std::max({ fee1, fee2, fee3 });
        }
        case mode::conservative:
        {
            const auto target1 = to_half(target);
            const auto target2 = std::min(one, target);
            const auto target3 = std::min(large, two * target);
            const auto fee1 = compute(target1, confidence::low);
            const auto fee2 = compute(target2, confidence::mid);
            const auto fee3 = compute(target3, confidence::high);
            return std::max({ fee1, fee2, fee3 });
        }
        default:
        case mode::unknown:
        {
            return estimate_failed;
        }
    }
}

bool estimator::initialize(const std::atomic_bool& cancel, const query& query,
    size_t count) NOEXCEPT
{
    if (is_zero(count))
        return true;

    const auto top = query.get_top_confirmed();
    if (count > add1(top))
        return false;

    rates pooled{};
    rate_sets blocks{};
    const auto start = top - sub1(count);
    return query.get_branch_fees(cancel, blocks, start, count) &&
        query.get_pool_fees(pooled) && initialize(blocks, pooled, top);
}

bool estimator::push(const query& query) NOEXCEPT
{
    if (is_add_overflow(top_height(), one))
        return false;

    rates block{};
    const auto link = query.to_confirmed(add1(top_height()));
    return query.get_block_fees(block, link) && push(block);
}

bool estimator::pop(const query& query) NOEXCEPT
{
    if (is_subtract_overflow(top_height(), one))
        return false;

    rates block{};
    const auto link = query.to_confirmed(sub1(top_height()));
    return query.get_block_fees(block, link) && pop(block);
}

bool estimator::pool(const query& query, const database::tx_link& link) NOEXCEPT
{
    rate tx{};
    return query.get_tx_fees(tx, link) && pool(tx);
}

size_t estimator::top_height() const NOEXCEPT
{
    return fees_.top_height.load(std::memory_order_relaxed);
}

// protected
// ----------------------------------------------------------------------------

bool estimator::to_bin(size_t& out, const rate& tx) NOEXCEPT
{
    // std::log (replace static with constexpr in c++26).
    static const auto growth = std::log(sizing::step);

    if (is_zero(tx.fee))
        return false;

    const auto rate = to_floating(tx.fee) / tx.bytes;
    if (rate < sizing::min)
        return false;

    // Clamp overflow to last bin.
    const auto bin = std::log(rate / sizing::min) / growth;
    out = std::min(to_floored_integer(bin), sub1(sizing::count));
    return true;
}

estimator::accumulator& estimator::history() NOEXCEPT
{
    return fees_;
}

const estimator::accumulator& estimator::history() const NOEXCEPT
{
    return fees_;
}

// Blocks end at the top height, pooled txs are entered as unconfirmed.
bool estimator::initialize(const rate_sets& blocks, const rates& pooled,
    size_t top) NOEXCEPT
{
    const auto count = blocks.size();
    if (count > add1(top))
        return false;

    fees_.top_height.store(top, std::memory_order_relaxed);
    auto height = add1(top) - count;

    // 3-4 secs slower when parallel at 1008 blocks.
    for (const auto& block: blocks)
        if (!update(block, height++, true))
            return false;

    for (const auto& tx: pooled)
        if (!pool(tx))
            return false;

    return true;
}

// Blocks must be pushed in order (but independent of chain index).
bool estimator::push(const rates& block) NOEXCEPT
{
    decay(true);
    fees_.top_height.fetch_add(one, std::memory_order_relaxed);
    return update(block, top_height(), true);
}

// Blocks must be pushed in order (but independent of chain index).
bool estimator::pop(const rates& block) NOEXCEPT
{
    const auto result = update(block, top_height(), false);
    decay(false);
    fees_.top_height.fetch_sub(one, std::memory_order_relaxed);
    return result;
}

// A tx is pending from its pool entry height until confirmed.
bool estimator::pool(const rate& tx) NOEXCEPT
{
    if (is_zero(tx.bytes))
        return false;

    // The store does not pool, so there is no entry height.
    if (tx.height == max_size_t)
        return true;

    size_t bin{};
    if (to_bin(bin, tx))
        pend(bin, tx.height, true);

    return true;
}

uint64_t estimator::compute(size_t target, double confidence,
    bool geometric) const NOEXCEPT
{
    const auto threshold = [](double part, double total, size_t) NOEXCEPT
    {
        return part / total;
    };

    // Geometric distribution approximation, not a full Markov process.
    const auto markov = [](double part, double total, size_t target) NOEXCEPT
    {
        return power(part / total, target);
    };

    // Pending txs that have waited beyond the target have failed it.
    bins failed{};
    pending(failed, target);

    const auto call = [&](const auto& buckets) NOEXCEPT
    {
        BC_PUSH_WARNING(NO_UNGUARDED_POINTERS)
        const auto& contribution = geometric ? markov : threshold;
        BC_POP_WARNING()

        constexpr auto magic_number = 2u;
        const auto at_least_four = magic_number * add1(target);
        double total{}, part{};
        auto index = buckets.size();
        auto found = index;
        for (const auto& bucket: std::views::reverse(buckets))
        {
            --index;
            total += to_floating(bucket.total + failed.at(index));
            part += to_floating(bucket.confirmed.at(target) + failed.at(index));
            if (total < at_least_four)
                continue;

            if (contribution(part, total, target) > (1.0 - confidence))
                break;

            found = index;
        }

        if (found == buckets.size())
            return max_uint64;

        const auto minimum = sizing::min * std::pow(sizing::step, found);
        return to_ceilinged_integer<uint64_t>(minimum);
    };

    if (target < horizon::small)  return call(fees_.small);
    if (target < horizon::medium) return call(fees_.medium);
    if (target < horizon::large)  return call(fees_.large);
    return max_uint64;
}

// private
// ----------------------------------------------------------------------------

void estimator::decay(bool push) NOEXCEPT
{
    const auto factor = to_scale_factor(push);
    decay(fees_.large, factor);
    decay(fees_.medium, factor);
    decay(fees_.small, factor);
}

void estimator::decay(auto& buckets, double factor) NOEXCEPT
{
    for (auto& bucket: buckets)
    {
        bucket.total = to_floored_integer(bucket.total * factor);
        for (auto& count: bucket.confirmed)
            count = to_floored_integer(count * factor);
    }
}

bool estimator::update(const rates& block, size_t height, bool push) NOEXCEPT
{
    constexpr auto large = maximum_horizon;
    bins counts{};

    for (const auto& tx: block)
    {
        if (is_zero(tx.bytes))
            return false;

        // A tx not pooled before its block has no wait.
        if (tx.height == max_size_t)
            continue;

        size_t bin{};
        if (!to_bin(bin, tx))
            continue;

        // Blocks waited beyond the first eligible, capped at the horizon.
        const auto waited = std::min(floored_subtract(height, tx.height), large);
        ++counts.at(bin);
        ++waits_.at(bin).at(waited);
        pend(bin, tx.height, !push);
    }

    // At age zero scale term is one.
    const auto age = top_height() - height;
    const auto scale = to_scale_term(age);
    const auto term = [&](size_t count) NOEXCEPT
    {
        const auto scaled = to_floored_integer(count * scale);
        return push ? scaled : twos_complement(scaled);
    };

    const auto call = [&](auto& buckets) NOEXCEPT
    {
        // The array count of the buckets element type.
        const auto horizon = buckets.front().confirmed.size();

        for (size_t bin{}; bin < counts.size(); ++bin)
        {
            const auto count = counts.at(bin);
            if (is_zero(count))
                continue;

            auto& bucket = buckets.at(bin);
            const auto& wait = waits_.at(bin);
            bucket.total += term(count);

            // Txs that waited beyond the target failed it.
            size_t failing{};
            for (auto waited = add1(horizon); waited <= large; ++waited)
                failing += wait.at(waited);

            for (auto target = horizon; !is_zero(target); --target)
            {
                failing += wait.at(target);
                bucket.confirmed.at(sub1(target)) += term(failing);
            }
        }
    };

    call(fees_.large);
    call(fees_.medium);
    call(fees_.small);

    for (size_t bin{}; bin < counts.size(); ++bin)
        if (!is_zero(counts.at(bin)))
            waits_.at(bin).fill(zero);

    return true;
}

// A slot holds the txs of one entry height, displacing those of an earlier.
void estimator::pend(size_t bin, size_t height, bool enter) NOEXCEPT
{
    const auto slot = height % horizon::large;
    auto& entry = fees_.entries.at(slot);
    auto& counts = fees_.pending.at(slot);

    if (entry != height)
    {
        // Displaced to aged (leaving), or displaced by the slot (entering).
        if (!enter || (entry > height))
        {
            auto& aged = fees_.aged.at(bin);
            aged = enter ? add1(aged) : floored_subtract(aged, one);
            return;
        }

        for (size_t index{}; index < counts.size(); ++index)
            fees_.aged.at(index) += counts.at(index);

        counts.fill(zero);
        entry = height;
    }

    auto& count = counts.at(bin);
    count = enter ? add1(count) : floored_subtract(count, one);
}

// Pending txs by bin that have waited beyond the target blocks.
void estimator::pending(bins& failed, size_t target) const NOEXCEPT
{
    const auto top = top_height();
    for (size_t slot{}; slot < horizon::large; ++slot)
    {
        // Eligible blocks passed without confirmation.
        const auto entry = fees_.entries.at(slot);
        const auto waited = (top < entry) ? zero : add1(top - entry);
        if (waited <= target)
            continue;

        const auto& counts = fees_.pending.at(slot);
        for (size_t bin{}; bin < counts.size(); ++bin)
            failed.at(bin) += counts.at(bin);
    }

    for (size_t bin{}; bin < failed.size(); ++bin)
        failed.at(bin) += fees_.aged.at(bin);
}

} // namespace node
} // namespace libbitcoin
