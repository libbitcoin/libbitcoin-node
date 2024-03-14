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
    log::settings configuration{};
    ////BOOST_REQUIRE_EQUAL(configuration.verbose, false);
    BOOST_REQUIRE_EQUAL(configuration.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(configuration.path, "");
    BOOST_REQUIRE_EQUAL(configuration.log_file1(), "bn_end.log");
    BOOST_REQUIRE_EQUAL(configuration.log_file2(), "bn_begin.log");
    BOOST_REQUIRE_EQUAL(configuration.events_file(), "events.log");
}

// [node]

BOOST_AUTO_TEST_CASE(settings__node__default_context__expected)
{
    node::settings configuration{};
    BOOST_REQUIRE_EQUAL(configuration.headers_first, true);
    BOOST_REQUIRE_EQUAL(configuration.allowed_deviation, 1.5);
    BOOST_REQUIRE_EQUAL(configuration.maximum_inventory, 8000);
    BOOST_REQUIRE_EQUAL(configuration.sample_period_seconds, 10_u16);
    BOOST_REQUIRE_EQUAL(configuration.currency_window_minutes, 60_u32);
    BOOST_REQUIRE(configuration.sample_period() == network::steady_clock::duration(network::seconds(10)));
    BOOST_REQUIRE(configuration.currency_window() == network::steady_clock::duration(network::minutes(60)));
}

BOOST_AUTO_TEST_SUITE_END()
