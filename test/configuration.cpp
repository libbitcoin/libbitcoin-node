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

using namespace bc::system;
using namespace bc::network;

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const node::configuration instance(chain::selection::none);

    // Just a sample of settings.
    BOOST_REQUIRE_EQUAL(instance.network.threads, 0u);
    BOOST_REQUIRE_EQUAL(instance.bitcoin.first_version, 1u);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__mainnet__success_peer_identifier_mainnet_magic)
{
    node::configuration instance(chain::selection::mainnet);
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.network.peer.identifier, 0xd9b4bef9_u32);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__silent_without_witness__invalid_configuration)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.silent_start_height = 0;
    instance.node.require_witness = false;
    BOOST_REQUIRE_EQUAL(instance.initialize(), network::error::invalid_configuration);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__silent_with_witness__success)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.silent_start_height = 0;
    instance.node.require_witness = true;
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_SUITE_END()
