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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(block_tests)

using namespace network::messages;

// The message holds a block_view and serializes in the requested form. Genesis
// is non-witness (witnessed and stripped forms are identical); the witnessed
// strip is covered by the libbitcoin-system block_view to_data tests.

BOOST_AUTO_TEST_CASE(block__serialize__witness__expected)
{
    const system::settings settings{ system::chain::selection::mainnet };
    const node::messages::block instance
    {
        { settings.genesis_block.to_data(true), true }
    };
    system::data_chunk buffer(instance.size(peer::level::canonical, true));
    BOOST_REQUIRE(instance.serialize(peer::level::canonical, { buffer }, true));
    BOOST_REQUIRE_EQUAL(buffer, settings.genesis_block.to_data(true));
}

BOOST_AUTO_TEST_CASE(block__serialize__non_witness__expected)
{
    const system::settings settings{ system::chain::selection::mainnet };
    const node::messages::block instance
    {
        { settings.genesis_block.to_data(true), false }
    };
    system::data_chunk buffer(instance.size(peer::level::canonical, false));
    BOOST_REQUIRE(instance.serialize(peer::level::canonical, { buffer }, false));
    BOOST_REQUIRE_EQUAL(buffer, settings.genesis_block.to_data(false));
}

BOOST_AUTO_TEST_CASE(block__serialize__short_buffer__false)
{
    const system::settings settings{ system::chain::selection::mainnet };
    const node::messages::block instance
    {
        { settings.genesis_block.to_data(true), true }
    };
    system::data_chunk buffer(sub1(instance.size(peer::level::canonical, true)));
    BOOST_REQUIRE(!instance.serialize(peer::level::canonical, { buffer }, true));
}

BOOST_AUTO_TEST_SUITE_END()
