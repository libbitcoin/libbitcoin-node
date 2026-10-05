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
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(estimator_tests)

using namespace system;

struct acessor
  : node::estimator
{
    typedef std::shared_ptr<acessor> ptr;

    static acessor::ptr create() NOEXCEPT
    {
        return { new acessor(),[](acessor* ptr) NOEXCEPT { delete ptr; } };
    }

    using rate = estimator::rate;
    using rates = estimator::rates;
    using rate_sets = estimator::rate_sets;
    using confidence = estimator::confidence;
    using horizon = estimator::horizon;
    using sizing = estimator::sizing;
    using estimator::decay_rate;
    using estimator::to_scale_term;
    using estimator::to_scale_factor;
    using estimator::to_bin;
    using estimator::history;
    using estimator::initialize;
    using estimator::push;
    using estimator::pop;
    using estimator::pool;
    using estimator::compute;
};

// Rate of 1/10 (0.1) is bin 0.
constexpr size_t bin0_bytes = 10;
constexpr uint64_t bin0_fee = 1;

// decay_rate

BOOST_AUTO_TEST_CASE(estimator__decay_rate__invoke__expected)
{
    const auto expected = std::pow(0.5, 1.0 / acessor::sizing::count);
    BOOST_REQUIRE_CLOSE(acessor::decay_rate(), expected, 0.000001);
}

// to_scale_term

BOOST_AUTO_TEST_CASE(estimator__to_scale_term__zero__one)
{
    BOOST_REQUIRE_EQUAL(acessor::to_scale_term(0u), 1.0);
}

BOOST_AUTO_TEST_CASE(estimator__to_scale_term__non_zero__expected)
{
    const auto rate = acessor::decay_rate();
    constexpr auto age = 42u;
    const auto expected = std::pow(rate, age);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_term(age), expected, 0.000001);
}

// to_scale_factor

BOOST_AUTO_TEST_CASE(estimator__to_scale_factor__push_true__decay_rate)
{
    const auto rate = acessor::decay_rate();
    const auto expected = std::pow(rate, +1.0);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_factor(true), expected, 0.000001);
}

BOOST_AUTO_TEST_CASE(estimator__to_scale_factor__push_false__inverse_decay_rate)
{
    const auto rate = acessor::decay_rate();
    const auto expected = std::pow(rate, -1.0);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_factor(false), expected, 0.000001);
}

// top_height

BOOST_AUTO_TEST_CASE(estimator__top_height__default__zero)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__top_height__non_default__expected)
{
    const auto instance = acessor::create();
    instance->history().top_height = 42u;
    BOOST_REQUIRE_EQUAL(instance->top_height(), 42u);
}

// estimate

BOOST_AUTO_TEST_CASE(estimator__estimate__unknown_mode__max_uint64)
{
    const auto instance = acessor::create();
    const auto estimate = instance->estimate(0u, estimator::mode::unknown);

    BOOST_REQUIRE_EQUAL(estimate, max_uint64);
}

// to_bin

BOOST_AUTO_TEST_CASE(estimator__to_bin__zero_fee__false)
{
    size_t bin{ 42 };
    BOOST_REQUIRE(!acessor::to_bin(bin, { bin0_bytes, 0u, 1u }));
}

BOOST_AUTO_TEST_CASE(estimator__to_bin__below_minimum_rate__false)
{
    size_t bin{ 42 };
    BOOST_REQUIRE(!acessor::to_bin(bin, { 100u, bin0_fee, 1u }));
}

BOOST_AUTO_TEST_CASE(estimator__to_bin__minimum_rate__bin_zero)
{
    size_t bin{ 42 };
    BOOST_REQUIRE(acessor::to_bin(bin, { bin0_bytes, bin0_fee, 1u }));
    BOOST_REQUIRE_EQUAL(bin, 0u);
}

BOOST_AUTO_TEST_CASE(estimator__to_bin__one_step__bin_one)
{
    size_t bin{ 42 };
    BOOST_REQUIRE(acessor::to_bin(bin, { 1000u, 106u, 1u }));
    BOOST_REQUIRE_EQUAL(bin, 1u);
}

BOOST_AUTO_TEST_CASE(estimator__to_bin__maximum_rate__last_bin)
{
    size_t bin{ 42 };
    BOOST_REQUIRE(acessor::to_bin(bin, { 1u, max_uint64, 1u }));
    BOOST_REQUIRE_EQUAL(bin, sub1(acessor::sizing::count));
}

// initialize

BOOST_AUTO_TEST_CASE(estimator__initialize__empty__true_height_unchanged)
{
    const auto instance = acessor::create();
    const acessor::rate_sets blocks{};
    const acessor::rates pooled{};
    BOOST_REQUIRE(instance->initialize(blocks, pooled, 0));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__more_blocks_than_height__false_height_unchanged)
{
    const auto instance = acessor::create();
    const acessor::rate_sets blocks(3);
    BOOST_REQUIRE(!instance->initialize(blocks, {}, 1));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__two_blocks__true_top_height)
{
    const auto instance = acessor::create();
    const acessor::rate_sets blocks(2);
    BOOST_REQUIRE(instance->initialize(blocks, {}, 5));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 5u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__unpooled_tx__not_counted)
{
    const auto instance = acessor::create();
    const acessor::rates block{ { bin0_bytes, bin0_fee } };
    const acessor::rate_sets blocks{ block };
    BOOST_REQUIRE(instance->initialize(blocks, {}, 1));
    BOOST_REQUIRE_EQUAL(instance->history().small.at(0).total, 0u);
    BOOST_REQUIRE_EQUAL(instance->history().medium.at(0).total, 0u);
    BOOST_REQUIRE_EQUAL(instance->history().large.at(0).total, 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__single_block_no_wait__populates_expected)
{
    const auto instance = acessor::create();

    // Pooled at height 1 and confirmed at height 1, no wait.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 1u } };
    const acessor::rate_sets blocks{ block };
    BOOST_REQUIRE(instance->initialize(blocks, {}, 1));

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, 1u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], 0u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__single_block_waited__fails_shorter_targets)
{
    const auto instance = acessor::create();

    // Pooled at height 3 and confirmed at height 5, waited 2 blocks.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 3u } };
    const acessor::rate_sets blocks{ block };
    BOOST_REQUIRE(instance->initialize(blocks, {}, 5));

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, 1u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[1], 1u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], 0u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1], 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__waited_beyond_horizon__fails_all_targets)
{
    const auto instance = acessor::create();

    // Pooled at height 1 and confirmed at height 2000, beyond the horizon.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 1u } };
    const acessor::rate_sets blocks{ block };
    BOOST_REQUIRE(instance->initialize(blocks, {}, 2000));

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 1u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 1u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__two_blocks_with_data__oldest_decayed)
{
    // Oldest: 1 tx, floor(1 * decay_rate) = 0. Newest: 2 txs, 2 * 1.0 = 2.
    const auto instance = acessor::create();
    const acessor::rates oldest{ { bin0_bytes, bin0_fee, 1u } };
    const acessor::rates newest{ { bin0_bytes, bin0_fee, 2u }, { bin0_bytes, bin0_fee, 2u } };
    const acessor::rate_sets blocks{ oldest, newest };
    BOOST_REQUIRE(instance->initialize(blocks, {}, 2));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 2u);
    BOOST_REQUIRE_EQUAL(instance->history().small.at(0).total, 2u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__pooled__pending_at_entry_height)
{
    const auto instance = acessor::create();
    const acessor::rate_sets blocks{};
    const acessor::rates pooled{ { bin0_bytes, bin0_fee, 3u }, { bin0_bytes, bin0_fee, 3u }, { bin0_bytes, bin0_fee, 7u } };
    BOOST_REQUIRE(instance->initialize(blocks, pooled, 2));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(3), 3u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(3).at(0), 2u);
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(7), 7u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(7).at(0), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__pooled_zero_bytes__false)
{
    const auto instance = acessor::create();
    const acessor::rate_sets blocks{};
    const acessor::rates pooled{ { 0u, bin0_fee, 3u } };
    BOOST_REQUIRE(!instance->initialize(blocks, pooled, 2));
}

// pool

BOOST_AUTO_TEST_CASE(estimator__pool__zero_bytes__false)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(!instance->pool({ 0u, bin0_fee, 1u }));
}

BOOST_AUTO_TEST_CASE(estimator__pool__unpooled__true_not_pending)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee }));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(0).at(0), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__pool__below_minimum_rate__true_not_pending)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->pool({ 100u, bin0_fee, 1u }));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(1), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__pool__pooled__pending)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 1u }));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(1), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 1u);
}

BOOST_AUTO_TEST_CASE(estimator__pool__later_height_same_slot__displaces_to_aged)
{
    const auto instance = acessor::create();
    constexpr auto large = acessor::horizon::large;
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 1u }));
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, add1(large) }));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(1), add1(large));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 1u);
}

BOOST_AUTO_TEST_CASE(estimator__pool__earlier_height_same_slot__aged)
{
    const auto instance = acessor::create();
    constexpr auto large = acessor::horizon::large;
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, add1(large) }));
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 1u }));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(1), add1(large));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 1u);
}

// push

BOOST_AUTO_TEST_CASE(estimator__push__empty_block__decays_and_increments)
{
    const auto instance = acessor::create();
    constexpr auto initial = 100u;
    instance->history().small[0].total = initial;
    const auto factor = acessor::to_scale_factor(true);
    const auto expected = to_floored_integer<size_t>(initial * factor);
    const acessor::rates empty{};
    BOOST_REQUIRE(instance->push(empty));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, expected);
}

BOOST_AUTO_TEST_CASE(estimator__push__single_tx_no_wait__populates_expected)
{
    const auto instance = acessor::create();

    // Pooled at height 1 and confirmed at height 1, no wait.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, 1u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], 0u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 1u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 0u);
}

BOOST_AUTO_TEST_CASE(estimator__push__single_tx_waited__fails_shorter_targets)
{
    const auto instance = acessor::create();
    instance->history().top_height = 4u;

    // Pooled at height 2 and confirmed at height 5, waited 3 blocks.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 2u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 5u);

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], 1u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[3], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);
}

BOOST_AUTO_TEST_CASE(estimator__push__pending_tx__leaves_pending)
{
    const auto instance = acessor::create();
    instance->history().top_height = 4u;
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 5u }));
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 5u }));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(5).at(0), 2u);

    const acessor::rates block{ { bin0_bytes, bin0_fee, 5u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(5).at(0), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().small.at(0).total, 1u);
}

BOOST_AUTO_TEST_CASE(estimator__push__aged_tx__leaves_aged)
{
    const auto instance = acessor::create();
    constexpr auto large = acessor::horizon::large;
    instance->history().top_height = large;
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 1u }));
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, add1(large) }));
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 1u);

    const acessor::rates block{ { bin0_bytes, bin0_fee, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().large.at(0).total, 1u);
    BOOST_REQUIRE_EQUAL(instance->history().large.at(0).confirmed[1007], 1u);
}

BOOST_AUTO_TEST_CASE(estimator__push__unpending_tx__saturates)
{
    const auto instance = acessor::create();
    const acessor::rates block{ { bin0_bytes, bin0_fee, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(1).at(0), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().aged.at(0), 0u);
}

// pop

BOOST_AUTO_TEST_CASE(estimator__pop__empty_block__decays_and_decrements)
{
    const auto instance = acessor::create();
    instance->history().top_height = 1u;
    constexpr auto initial = 100u;
    instance->history().small[0].total = initial;
    const auto factor = acessor::to_scale_factor(false);
    const auto expected = to_floored_integer<size_t>(initial * factor);
    const acessor::rates empty{};
    BOOST_REQUIRE(instance->pop(empty));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, expected);
}

BOOST_AUTO_TEST_CASE(estimator__pop__reverses_push__restores_buckets_and_pending)
{
    const auto instance = acessor::create();
    instance->history().top_height = 4u;
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 2u }));

    // Pooled at height 2 and confirmed at height 5, waited 3 blocks.
    const acessor::rates block{ { bin0_bytes, bin0_fee, 2u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(2).at(0), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().small.at(0).confirmed[2], 1u);

    BOOST_REQUIRE(instance->pop(block));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 4u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(2).at(0), 1u);

    const auto& small0 = instance->history().small.at(0);
    BOOST_REQUIRE_EQUAL(small0.total, 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], 0u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 0u);
}

// compute

BOOST_AUTO_TEST_CASE(estimator__compute__default_state__max_uint64)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
    BOOST_REQUIRE_EQUAL(instance->compute(1, acessor::confidence::mid, true), max_uint64);
    BOOST_REQUIRE_EQUAL(instance->compute(50, acessor::confidence::low), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__insufficient_total__max_uint64)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;

    // < at_least_four=2 for target=0.
    constexpr auto value = 1u;
    instance->history().small[bin].total = value;
    instance->history().small[bin].confirmed[0] = value;
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__low_failure_basic__expected_fee)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // 0/10 = 0 <= 0.05.
    constexpr auto failure = 0u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[0] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__high_failure_basic__max_uint64)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // 1/10 = 0.1 > 0.05.
    constexpr auto failure = 1u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[0] = failure;
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__multi_bin_basic__expected_fee)
{
    const auto instance = acessor::create();
    constexpr auto low_bin = 0u;
    constexpr auto high_bin = 1u;
    constexpr auto total = 10u;

    // high failure in low bin.
    constexpr auto low_failure = 10u;

    // low failure in high bin.
    constexpr auto high_failure = 0u;
    instance->history().small[low_bin].total = total;
    instance->history().small[low_bin].confirmed[0] = low_failure;
    instance->history().small[high_bin].total = total;
    instance->history().small[high_bin].confirmed[0] = high_failure;

    // Cumulative at high_bin: 0/10 = 0 <= 0.05, then at low_bin: 10/20 = 0.5 > 0.05, found=1.
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, high_bin));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__pending_within_target__not_counted)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;
    instance->history().small[bin].total = total;
    instance->history().top_height = 5u;

    // Pooled at height 6 with top at 5, no blocks waited.
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 6u }));
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__pending_beyond_target__failed)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;
    instance->history().small[bin].total = total;
    instance->history().top_height = 5u;

    // Pooled at height 5 with top at 5, one block waited: 1/11 > 0.05.
    BOOST_REQUIRE(instance->pool({ bin0_bytes, bin0_fee, 5u }));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);

    // Target 1 (two blocks) is not yet failed.
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(1, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__aged__failed_all_targets)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;
    instance->history().small[bin].total = total;
    instance->history().aged[bin] = 1u;
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
    BOOST_REQUIRE_EQUAL(instance->compute(11, acessor::confidence::high), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__geometric_target_one__matches_basic)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;
    constexpr auto failure = 0u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[1] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    const auto basic = instance->compute(1, acessor::confidence::high, false);
    const auto geometric = instance->compute(1, acessor::confidence::high, true);
    BOOST_REQUIRE_EQUAL(basic, fee);
    BOOST_REQUIRE_EQUAL(geometric, fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__geometric_high_target__expected)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // p=0.1, pow(0.1,2)=0.01 < 0.05, so found=0.
    constexpr auto failure = 1u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[2] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(2, acessor::confidence::high, true), fee);

    // Contrast with basic: 0.1 > 0.05, would be max_uint64.
    BOOST_REQUIRE_EQUAL(instance->compute(2, acessor::confidence::high, false), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__large_target__max_uint64)
{
    const auto instance = acessor::create();
    instance->history().large[0].total = 10'000u;
    BOOST_REQUIRE_EQUAL(instance->compute(acessor::horizon::large, acessor::confidence::low), max_uint64);
}

// estimate (modes)

BOOST_AUTO_TEST_CASE(estimator__estimate__large_target__max_uint64)
{
    const auto instance = acessor::create();
    instance->history().large[0].total = 10'000u;
    BOOST_REQUIRE_EQUAL(instance->estimate(acessor::horizon::large, estimator::mode::basic), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__basic__expected)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    instance->history().small[0].confirmed[2] = 1u;
    BOOST_REQUIRE_EQUAL(instance->estimate(1, estimator::mode::basic), 1u);
    BOOST_REQUIRE_EQUAL(instance->estimate(2, estimator::mode::basic), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__geometric__expected)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    instance->history().small[0].confirmed[2] = 1u;
    BOOST_REQUIRE_EQUAL(instance->estimate(2, estimator::mode::geometric), 1u);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__economical_double_target_failed__max_uint64)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    instance->history().small[0].confirmed[2] = 1u;
    BOOST_REQUIRE_EQUAL(instance->estimate(1, estimator::mode::economical), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__economical_all_targets_pass__expected)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    BOOST_REQUIRE_EQUAL(instance->estimate(1, estimator::mode::economical), 1u);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__conservative_double_target_failed__max_uint64)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    instance->history().small[0].confirmed[2] = 1u;
    BOOST_REQUIRE_EQUAL(instance->estimate(1, estimator::mode::conservative), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__estimate__conservative_all_targets_pass__expected)
{
    const auto instance = acessor::create();
    instance->history().small[0].total = 10u;
    BOOST_REQUIRE_EQUAL(instance->estimate(1, estimator::mode::conservative), 1u);
}

// history

BOOST_AUTO_TEST_CASE(estimator__history__const__expected)
{
    const auto instance = acessor::create();
    instance->history().top_height = 42u;
    const auto& constant = *instance;
    BOOST_REQUIRE_EQUAL(constant.history().top_height, 42u);
}

// update

BOOST_AUTO_TEST_CASE(estimator__push__zero_bytes__false)
{
    const auto instance = acessor::create();
    const acessor::rates block{ { 0u, bin0_fee, 1u } };
    BOOST_REQUIRE(!instance->push(block));
}

BOOST_AUTO_TEST_CASE(estimator__push__zero_fee__not_counted)
{
    const auto instance = acessor::create();
    const acessor::rates block{ { bin0_bytes, 0u, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, 0u);
}

BOOST_AUTO_TEST_CASE(estimator__push__below_minimum_rate__not_counted)
{
    const auto instance = acessor::create();
    const acessor::rates block{ { 100u, bin0_fee, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, 0u);
}

// query

struct estimator_query_setup_fixture
{
    DELETE_COPY_MOVE(estimator_query_setup_fixture);

    static database::settings configure() NOEXCEPT
    {
        database::settings settings{};
        settings.path = TEST_DIRECTORY;
        return settings;
    }

    estimator_query_setup_fixture()
      : settings_{ configure() }, store_{ settings_ }, query_{ store_ }
    {
        BOOST_REQUIRE(test::clear(test::directory));
        const auto ec = store_.create([](auto, auto) NOEXCEPT {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        const system::settings bitcoin{ chain::selection::mainnet };
        BOOST_REQUIRE(query_.initialize(bitcoin.genesis_block));
    }

    ~estimator_query_setup_fixture()
    {
        const auto ec = store_.close([](auto, auto) NOEXCEPT {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    const database::settings settings_;
    node::store store_;
    node::query query_;
    const std::atomic_bool cancel_{};
};

BOOST_FIXTURE_TEST_CASE(estimator__initialize__query_zero_count__true_height_unchanged, estimator_query_setup_fixture)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->initialize(cancel_, query_, 0));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

BOOST_FIXTURE_TEST_CASE(estimator__initialize__query_count_exceeds_chain__false, estimator_query_setup_fixture)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(!instance->initialize(cancel_, query_, 2));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

BOOST_FIXTURE_TEST_CASE(estimator__push__query_maximum_top_height__false, estimator_query_setup_fixture)
{
    const auto instance = acessor::create();
    instance->history().top_height = max_size_t;
    BOOST_REQUIRE(!instance->push(query_));
    BOOST_REQUIRE_EQUAL(instance->top_height(), max_size_t);
}

BOOST_FIXTURE_TEST_CASE(estimator__pop__query_zero_top_height__false, estimator_query_setup_fixture)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(!instance->pop(query_));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

static chain::block estimator_block(const chain::block& parent,
    uint32_t height, const chain::transactions& txs)
{
    hashes ids{};
    for (const auto& tx: txs)
        ids.push_back(tx.hash(false));

    const auto previous = parent.hash();
    const auto root = sha256::merkle_root(std::move(ids));
    const auto timestamp = parent.header().timestamp() + 600u;
    const auto bits = parent.header().bits();
    const chain::header header{ 1u, previous, root, timestamp, bits, height };
    return { header, txs };
}

static chain::transaction estimator_coinbase(uint8_t tag, uint64_t value)
{
    const chain::point null{ null_hash, chain::point::null_index };
    const chain::script tagged{ data_chunk{ 0x01, tag }, false };
    const chain::script op_true{ data_chunk{ 0x51 }, false };
    const chain::input input{ null, tagged, max_uint32 };
    const chain::output output{ value, op_true };
    return { 1u, chain::inputs{ input }, chain::outputs{ output }, 0u };
}

static bool estimator_confirm(node::query& query, const chain::block& block,
    uint32_t height)
{
    const database::context context{ 0, height, block.header().timestamp() };
    if (!query.set(block, context, {}, false, true))
        return false;

    const auto link = query.to_header(block.hash());
    return query.push_candidate(link) && query.push_confirmed(link, true);
}

static size_t estimator_small_total(const acessor& instance)
{
    size_t total{};
    for (const auto& bucket: instance.history().small)
        total += bucket.total;

    return total;
}

static chain::transaction estimator_spend(const chain::transaction& parent,
    uint64_t fee)
{
    const chain::input input{ chain::point{ parent.hash(false), 0 }, chain::script{}, max_uint32 };
    const chain::output output{ parent.outputs_ptr()->front()->value() - fee, chain::script{ data_chunk{ 0x51 }, false } };
    return { 1u, chain::inputs{ input }, chain::outputs{ output }, 0u };
}

static bool estimator_pool(node::store& store, node::query& query,
    const chain::transaction& tx, uint32_t height, uint64_t fee)
{
    const database::context context{ 0, height, 0 };
    return store.pool.put(query.to_tx(tx.hash(false)), database::table::pool::record{ {}, context, fee, {}, {} });
}

static size_t estimator_bin(const chain::transaction& tx, uint64_t fee)
{
    size_t bin{};
    BOOST_REQUIRE(acessor::to_bin(bin, { tx.virtual_size(), fee, 0u }));
    return bin;
}

BOOST_FIXTURE_TEST_CASE(estimator__initialize__query_unpooled_block_tx__not_counted, estimator_query_setup_fixture)
{
    constexpr uint64_t value = 5'000'000'000;
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block;
    const auto coinbase1 = estimator_coinbase(1, value);
    const auto spend = estimator_spend(coinbase1, 10'000u);
    const auto block1 = estimator_block(genesis, 1, { coinbase1 });
    const auto block2 = estimator_block(block1, 2, { estimator_coinbase(2, value), spend });
    BOOST_REQUIRE(estimator_confirm(query_, block1, 1));
    BOOST_REQUIRE(estimator_confirm(query_, block2, 2));

    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->initialize(cancel_, query_, 3));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 2u);
    BOOST_REQUIRE_EQUAL(estimator_small_total(*instance), 0u);
}

BOOST_FIXTURE_TEST_CASE(estimator__initialize__query_unconfirmed_pooled__pending, estimator_query_setup_fixture)
{
    constexpr uint64_t value = 5'000'000'000;
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block;
    const auto coinbase1 = estimator_coinbase(1, value);
    const auto spend = estimator_spend(coinbase1, 10'000u);
    const auto block1 = estimator_block(genesis, 1, { coinbase1 });
    const auto block2 = estimator_block(block1, 2, { estimator_coinbase(2, value), spend });
    BOOST_REQUIRE(estimator_confirm(query_, block1, 1));

    // Block 2 is archived but not confirmed, so the spend is pending.
    BOOST_REQUIRE(query_.set(block2, database::context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(estimator_pool(store_, query_, spend, 2, 10'000u));

    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->initialize(cancel_, query_, 2));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);
    BOOST_REQUIRE_EQUAL(estimator_small_total(*instance), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(2), 2u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(2).at(estimator_bin(spend, 10'000u)), 1u);
}

BOOST_FIXTURE_TEST_CASE(estimator__pool__query_pooled_link__pending, estimator_query_setup_fixture)
{
    constexpr uint64_t value = 5'000'000'000;
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block;
    const auto coinbase1 = estimator_coinbase(1, value);
    const auto spend = estimator_spend(coinbase1, 10'000u);
    const auto block1 = estimator_block(genesis, 1, { coinbase1 });
    const auto block2 = estimator_block(block1, 2, { estimator_coinbase(2, value), spend });
    BOOST_REQUIRE(estimator_confirm(query_, block1, 1));
    BOOST_REQUIRE(query_.set(block2, database::context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(estimator_pool(store_, query_, spend, 2, 10'000u));

    const auto instance = acessor::create();
    BOOST_REQUIRE(instance->pool(query_, query_.to_tx(spend.hash(false))));
    BOOST_REQUIRE_EQUAL(instance->history().entries.at(2), 2u);
    BOOST_REQUIRE_EQUAL(instance->history().pending.at(2).at(estimator_bin(spend, 10'000u)), 1u);
}

BOOST_FIXTURE_TEST_CASE(estimator__pool__query_unknown_link__false, estimator_query_setup_fixture)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE(!instance->pool(query_, 42));
}

BOOST_AUTO_TEST_SUITE_END()
