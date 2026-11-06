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

BC_AUTO_TEST_SUITE(rolling_bloom_filter_tests)

constexpr auto generation = network::messages::peer::max_inventory;
constexpr auto generations2 = system::multiply(two, generation);
constexpr auto generations3 = system::multiply(3_size, generation);

// A distinct uniform hash for each index.
static system::hash_digest to_hash(size_t index)
{
    using namespace system;
    return sha256_hash(to_little_endian<uint64_t>(index));
}

// The hashes of [first, first + count).
static system::hashes to_hashes(size_t first, size_t count)
{
    std_vector<size_t> indexes(count);
    std::iota(indexes.begin(), indexes.end(), first);
    system::hashes out(count);
    std::ranges::transform(indexes, out.begin(), to_hash);
    return out;
}

// Insert each of the hashes.
static void insert(rolling_bloom_filter& filter, const system::hashes& items)
{
    std::ranges::for_each(items, [&](const system::hash_digest& item) NOEXCEPT
    {
        filter.insert(item);
    });
}

// The number of the hashes contained.
static size_t contained(const rolling_bloom_filter& filter,
    const system::hashes& items)
{
    const auto count = [&](size_t total, const auto& item) NOEXCEPT
    {
        return total + to_int<size_t>(filter.contains(item));
    };

    return std::accumulate(items.cbegin(), items.cend(), zero, count);
}

BC_AUTO_TEST_CASE(rolling_bloom_filter__contains__empty__false)
{
    const rolling_bloom_filter filter{};
    const auto item = to_hash(0);
    BC_REQUIRE(!filter.contains(item));
}

BC_AUTO_TEST_CASE(rolling_bloom_filter__insert__one__contained)
{
    const auto item = to_hash(0);
    rolling_bloom_filter filter{};
    filter.insert(item);
    BC_REQUIRE(filter.contains(item));
}

BC_AUTO_TEST_CASE(rolling_bloom_filter__insert__three_generations__all_contained)
{
    const auto items = to_hashes(0, generations3);
    rolling_bloom_filter filter{};
    insert(filter, items);
    BC_REQUIRE_EQUAL(contained(filter, items), generations3);
}

BC_AUTO_TEST_CASE(rolling_bloom_filter__insert__over_three_generations__oldest_generation_evicted)
{
    const auto items = to_hashes(0, add1(generations3));
    const auto first = to_hashes(0, one);
    const auto oldest = to_hashes(0, generation);
    const auto newer = to_hashes(generation, add1(generations2));
    rolling_bloom_filter filter{};
    insert(filter, items);

    const auto first_count = contained(filter, first);
    const auto oldest_count = contained(filter, oldest);
    const auto newer_count = contained(filter, newer);
    BC_REQUIRE(is_zero(first_count));
    BC_REQUIRE_LE(oldest_count, two);
    BC_REQUIRE_EQUAL(newer_count, add1(generations2));
}

BC_AUTO_TEST_CASE(rolling_bloom_filter__contains__three_generations__false_positives_bounded)
{
    const auto items = to_hashes(0, generations3);
    const auto absent = to_hashes(generations3, 1'000'000);
    rolling_bloom_filter filter{};
    insert(filter, items);
    BC_REQUIRE_LE(contained(filter, absent), 10_size);
}

BC_AUTO_TEST_SUITE_END()
