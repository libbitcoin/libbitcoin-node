/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
    BOOST_REQUIRE_EQUAL(configuration.sync_peers, 8u);
    BOOST_REQUIRE_EQUAL(configuration.sync_timeout_seconds, 5u);
    BOOST_REQUIRE_EQUAL(configuration.refresh_transactions, true);
}

BOOST_AUTO_TEST_CASE(settings__construct__none_context__expected)
{
    node::settings configuration(config::settings::none);
    BOOST_REQUIRE_EQUAL(configuration.sync_peers, 8u);
    BOOST_REQUIRE_EQUAL(configuration.sync_timeout_seconds, 5u);
    BOOST_REQUIRE_EQUAL(configuration.refresh_transactions, true);
}

BOOST_AUTO_TEST_CASE(settings__construct__mainnet_context__expected)
{
    node::settings configuration(config::settings::mainnet);
    BOOST_REQUIRE_EQUAL(configuration.sync_peers, 8u);
    BOOST_REQUIRE_EQUAL(configuration.sync_timeout_seconds, 5u);
    BOOST_REQUIRE_EQUAL(configuration.refresh_transactions, true);
}

BOOST_AUTO_TEST_CASE(settings__construct__testnet_context__expected)
{
    node::settings configuration(config::settings::testnet);
    BOOST_REQUIRE_EQUAL(configuration.sync_peers, 8u);
    BOOST_REQUIRE_EQUAL(configuration.sync_timeout_seconds, 5u);
    BOOST_REQUIRE_EQUAL(configuration.refresh_transactions, true);
}

BOOST_AUTO_TEST_SUITE_END()
