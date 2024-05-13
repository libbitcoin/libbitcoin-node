/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

// [log]

BOOST_AUTO_TEST_CASE(settings__log__default_context__expected)
{
    const log::settings log{};
    ////BOOST_REQUIRE_EQUAL(log.verbose, false);
    BOOST_REQUIRE_EQUAL(log.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(log.path, "");
    BOOST_REQUIRE_EQUAL(log.log_file1(), "bn_end.log");
    BOOST_REQUIRE_EQUAL(log.log_file2(), "bn_begin.log");
    BOOST_REQUIRE_EQUAL(log.events_file(), "events.log");
}

// [node]

BOOST_AUTO_TEST_CASE(settings__node__default_context__expected)
{
    const node::settings node{};
    BOOST_REQUIRE_EQUAL(node.headers_first, true);
    BOOST_REQUIRE_EQUAL(node.allowed_deviation, 1.5);

    BOOST_REQUIRE_EQUAL(node.snapshot_bytes, 107'374'182'400_u64);
    BOOST_REQUIRE_EQUAL(node.snapshot_valid, 100'000_u32);

    BOOST_REQUIRE_EQUAL(node.maximum_height, 0_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);

    BOOST_REQUIRE_EQUAL(node.maximum_concurrency, 50000_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency_(), 50000_size);

    BOOST_REQUIRE_EQUAL(node.sample_period_seconds, 10_u16);
    BOOST_REQUIRE(node.sample_period() == network::steady_clock::duration(network::seconds(10)));

    BOOST_REQUIRE_EQUAL(node.currency_window_minutes, 60_u32);
    BOOST_REQUIRE(node.currency_window() == network::steady_clock::duration(network::minutes(60)));
}

BOOST_AUTO_TEST_SUITE_END()
