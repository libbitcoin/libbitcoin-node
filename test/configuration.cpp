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

BOOST_AUTO_TEST_SUITE(configuration_tests)

// constructors
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    node::configuration instance(bc::settings::none);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(instance.node.peers.empty());
    BOOST_REQUIRE_EQUAL(instance.node.download_connections, 8u);
    BOOST_REQUIRE_EQUAL(instance.node.block_timeout_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct1__mainnet_context__expected)
{
    node::configuration instance(bc::settings::mainnet);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(instance.node.peers.empty());
    BOOST_REQUIRE_EQUAL(instance.node.download_connections, 8u);
    BOOST_REQUIRE_EQUAL(instance.node.block_timeout_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct1__testnet_context__expected)
{
    node::configuration instance(bc::settings::testnet);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(instance.node.peers.empty());
    BOOST_REQUIRE_EQUAL(instance.node.download_connections, 8u);
    BOOST_REQUIRE_EQUAL(instance.node.block_timeout_seconds, 5u);
}

BOOST_AUTO_TEST_CASE(configuration__construct2__none_context__expected)
{
    node::configuration instance1(bc::settings::none);
    instance1.help = true;
    instance1.initchain = true;
    instance1.settings = true;
    instance1.version = true;
    instance1.node.peers.push_back({});
    instance1.node.download_connections = 42;
    instance1.node.block_timeout_seconds = 24;

    node::configuration instance2(instance1);

    BOOST_REQUIRE(instance2.help);
    BOOST_REQUIRE(instance2.initchain);
    BOOST_REQUIRE(instance2.settings);
    BOOST_REQUIRE(instance2.version);
    BOOST_REQUIRE_EQUAL(instance2.node.peers.size(), 1u);
    BOOST_REQUIRE_EQUAL(instance2.node.download_connections, 42u);
    BOOST_REQUIRE_EQUAL(instance2.node.block_timeout_seconds, 24u);
}

// last_checkpoint_height
//-----------------------------------------------------------------------------

// This class assumes that checkponts are previously sorted.
BOOST_AUTO_TEST_CASE(configuration__last_checkpoint_height__none_context__expected)
{
    const size_t expected = 42;
    node::configuration instance(bc::settings::none);
    instance.chain.checkpoints.push_back({ "0000000000000000000000000000000000000000000000000000000000000001", 1 });
    instance.chain.checkpoints.push_back({ "0000000000000000000000000000000000000000000000000000000000000002", 2 });
    instance.chain.checkpoints.push_back({ "0000000000000000000000000000000000000000000000000000000000000042", expected });
    BOOST_REQUIRE_EQUAL(instance.last_checkpoint_height(), expected);
}

BOOST_AUTO_TEST_SUITE_END()
