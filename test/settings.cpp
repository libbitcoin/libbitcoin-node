/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(settings_tests)

using namespace bc::network;
using namespace bc::system::chain;

// [node]

BOOST_AUTO_TEST_CASE(settings__node__default_context__expected)
{
    using namespace network;

    const node::settings node{};
    BOOST_REQUIRE_EQUAL(node.thread_priority, true);
    BOOST_REQUIRE_EQUAL(node.delay_inbound, true);
    BOOST_REQUIRE_EQUAL(node.headers_first, true);
    BOOST_REQUIRE_EQUAL(node.defer_validation, false);
    BOOST_REQUIRE_EQUAL(node.defer_confirmation, false);
    BOOST_REQUIRE_EQUAL(node.minimum_free_rate, 0.0);
    BOOST_REQUIRE_EQUAL(node.minimum_bump_rate, 0.0);
    BOOST_REQUIRE_EQUAL(node.allowed_deviation, 1.5);
    BOOST_REQUIRE_EQUAL(node.announcement_cache, 42_u16);
    BOOST_REQUIRE_EQUAL(node.allocation_multiple, 20_u16);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_bytes, 200'000'000'000_u64);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_valid, 250'000_u32);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_confirm, 500'000_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_height, 0_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency, 50000_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency_(), 50000_size);
    BOOST_REQUIRE_EQUAL(node.sample_period_seconds, 10_u16);
    BOOST_REQUIRE_EQUAL(node.currency_window_minutes, 60_u32);
    BOOST_REQUIRE_EQUAL(node.threads, 1_u32);

    BOOST_REQUIRE_EQUAL(node.threads_(), one);
    BOOST_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency_(), 50'000_size);
    BOOST_REQUIRE(node.sample_period() == steady_clock::duration(seconds(10)));
    BOOST_REQUIRE(node.currency_window() == steady_clock::duration(minutes(60)));
    BOOST_REQUIRE(node.thread_priority_() == network::processing_priority::high);
    BOOST_REQUIRE(node.memory_priority_() == network::memory_priority::highest);
}

BOOST_AUTO_TEST_SUITE_END()
