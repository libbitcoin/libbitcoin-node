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
#include "p2p_setup_fixture.hpp"

BOOST_FIXTURE_TEST_SUITE(functional_p2p_tests, p2p_setup_fixture)

using namespace network::messages::peer;

BOOST_AUTO_TEST_CASE(functional_p2p__handshake__default__provides_network_and_witness)
{
    BOOST_REQUIRE(handshake());
    BOOST_REQUIRE_EQUAL(node_version->value, config_.network.protocol_maximum);
    BOOST_REQUIRE_EQUAL(node_version->services,
        service::node_network | service::node_witness);
}

BOOST_AUTO_TEST_CASE(functional_p2p__ping__nonce__pong_echo)
{
    BOOST_REQUIRE(handshake());

    constexpr uint64_t expected = 42;
    send(ping{ expected }, node_version->value);

    const auto payload = receive(pong::command);
    const auto message = pong::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, expected);
}

// The block send regression (github.com/libbitcoin/libbitcoin-network/862).
BOOST_AUTO_TEST_CASE(functional_p2p__get_data__genesis_block__expected_bytes)
{
    BOOST_REQUIRE(handshake());

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto expected = genesis.to_data(true);

    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, node_version->value);

    const auto payload = receive(block::command);
    BOOST_REQUIRE_EQUAL(payload.size(), expected.size());
    BOOST_REQUIRE(payload == expected);
}

BOOST_AUTO_TEST_CASE(functional_p2p__get_data__unknown_block__not_found)
{
    BOOST_REQUIRE(handshake());

    const get_data get{ { { inventory_item::type_id::block, system::one_hash } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

// A limited node answers for a block it has pruned, and remains unfaulted.
struct p2p_limited_setup_fixture
  : p2p_setup_fixture
{
    p2p_limited_setup_fixture() NOEXCEPT
      : p2p_setup_fixture({}, [](configuration& config) NOEXCEPT
        {
            config.node.limited_blocks = true;
        })
    {
    }
};

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__pruned_block__not_found, p2p_limited_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
}

BOOST_AUTO_TEST_SUITE_END()
