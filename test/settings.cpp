/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <boost/test/unit_test.hpp>
#include <bitcoin/node.hpp>

using namespace bc;

BOOST_AUTO_TEST_SUITE(settings_tests)

// constructors
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(settings__construct__default_context__expected)
{
    node::settings configuration;
    BOOST_REQUIRE(!configuration.refresh_transactions);
    BOOST_REQUIRE_EQUAL(configuration.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(settings__construct__none_context__expected)
{
    node::settings configuration(config::settings::none);
    BOOST_REQUIRE(!configuration.refresh_transactions);
    BOOST_REQUIRE_EQUAL(configuration.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(settings__construct__mainnet_context__expected)
{
    node::settings configuration(config::settings::mainnet);
    BOOST_REQUIRE(!configuration.refresh_transactions);
    BOOST_REQUIRE_EQUAL(configuration.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(settings__construct__testnet_context__expected)
{
    node::settings configuration(config::settings::testnet);
    BOOST_REQUIRE(!configuration.refresh_transactions);
    BOOST_REQUIRE_EQUAL(configuration.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_SUITE_END()
