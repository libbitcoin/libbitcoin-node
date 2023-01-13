/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

using namespace bc::system;

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    node::configuration instance(chain::selection::none);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(!instance.node.refresh_transactions);
    BOOST_REQUIRE_EQUAL(instance.node.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct1__mainnet_context__expected)
{
    node::configuration instance(chain::selection::mainnet);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(!instance.node.refresh_transactions);
    BOOST_REQUIRE_EQUAL(instance.node.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct1__testnet_context__expected)
{
    node::configuration instance(chain::selection::testnet);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(!instance.node.refresh_transactions);
    BOOST_REQUIRE_EQUAL(instance.node.block_latency_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct2__none_context__expected)
{
    node::configuration instance1(chain::selection::none);
    instance1.help = true;
    instance1.initchain = true;
    instance1.settings = true;
    instance1.version = true;
    instance1.node.refresh_transactions = true;
    instance1.node.block_latency_seconds = 24;

    node::configuration instance2(instance1);

    BOOST_REQUIRE(instance2.help);
    BOOST_REQUIRE(instance2.initchain);
    BOOST_REQUIRE(instance2.settings);
    BOOST_REQUIRE(instance2.version);
    BOOST_REQUIRE(instance2.node.refresh_transactions);
    BOOST_REQUIRE_EQUAL(instance2.node.block_latency_seconds, 24u);
}

BOOST_AUTO_TEST_SUITE_END()
