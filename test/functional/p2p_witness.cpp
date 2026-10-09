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

BOOST_FIXTURE_TEST_SUITE(functional_p2p_witness_tests, p2p_witness_setup_fixture)

using namespace network::messages::peer;

static const system::hash_digest& genesis_coinbase() NOEXCEPT
{
    static const system::settings bitcoin{ system::chain::selection::mainnet };
    return bitcoin.genesis_block.transactions_ptr()->front()->get_hash(true);
}

// handshake

BOOST_AUTO_TEST_CASE(functional_p2p_witness__handshake__witness_tx__witness_tx_id_relay_before_acknowledge)
{
    version out{};
    out.value = level::bip339;
    out.timestamp = system::sign_cast<uint64_t>(network::zulu_time());
    out.nonce = 42424242;
    out.user_agent = "/test/";
    out.relay = true;
    send(out, level::bip339);

    BOOST_REQUIRE_EQUAL(receive().first, version::command);
    BOOST_REQUIRE_EQUAL(receive().first, witness_tx_id_relay::command);
    BOOST_REQUIRE_EQUAL(receive().first, version_acknowledge::command);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p_witness__handshake__witness_tx_disabled__no_witness_tx_id_relay, p2p_relay_setup_fixture)
{
    version out{};
    out.value = level::bip339;
    out.timestamp = system::sign_cast<uint64_t>(network::zulu_time());
    out.nonce = 42424242;
    out.user_agent = "/test/";
    out.relay = true;
    send(out, level::bip339);

    BOOST_REQUIRE_EQUAL(receive().first, version::command);
    BOOST_REQUIRE_EQUAL(receive().first, version_acknowledge::command);
}

// inventory (wtxid in)

BOOST_AUTO_TEST_CASE(functional_p2p_witness__inventory__unknown_wtxid__get_data_wtxid)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const inventory inv{ { { inventory_item::type_id::wtxid, system::one_hash } } };
    send(inv, node_version->value);

    const auto payload = receive(get_data::command);
    const auto message = get_data::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().is_type(inventory_item::type_id::wtxid));
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

BOOST_AUTO_TEST_CASE(functional_p2p_witness__inventory__known_wtxid__not_requested)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const inventory inv{ { { inventory_item::type_id::wtxid, genesis_coinbase() } } };
    send(inv, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE(!received(get_data::command, pong::command));
}

BOOST_AUTO_TEST_CASE(functional_p2p_witness__inventory__transaction_type__not_requested)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const inventory inv{ { { inventory_item::type_id::transaction, system::one_hash } } };
    send(inv, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE(!received(get_data::command, pong::command));
}

// The peer did not signal, so the transaction type is requested.
BOOST_AUTO_TEST_CASE(functional_p2p_witness__inventory__unsignaled_transaction__get_data_witness_tx)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, false));

    const inventory inv{ { { inventory_item::type_id::transaction, system::one_hash } } };
    send(inv, node_version->value);

    const auto payload = receive(get_data::command);
    const auto message = get_data::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().is_type(inventory_item::type_id::witness_tx));
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

BOOST_AUTO_TEST_CASE(functional_p2p_witness__inventory__unsignaled_wtxid__not_requested)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, false));

    const inventory inv{ { { inventory_item::type_id::wtxid, system::one_hash } } };
    send(inv, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE(!received(get_data::command, pong::command));
}

// transaction (wtxid in)

BOOST_AUTO_TEST_CASE(functional_p2p_witness__transaction__unrequested__stopped)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const system::settings bitcoin{ system::chain::selection::mainnet };
    const auto& coinbase = bitcoin.genesis_block.transactions_ptr()->front();
    send(transaction::command, coinbase->to_data(true));

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

// get_data (wtxid out)

BOOST_AUTO_TEST_CASE(functional_p2p_witness__get_data__unknown_wtxid__not_found_wtxid)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const get_data get{ { { inventory_item::type_id::wtxid, system::one_hash } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().is_type(inventory_item::type_id::wtxid));
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

BOOST_AUTO_TEST_CASE(functional_p2p_witness__get_data__unsegregated_wtxid__transaction)
{
    BOOST_REQUIRE(handshake(0, level::bip339, true, true));

    const get_data get{ { { inventory_item::type_id::wtxid, genesis_coinbase() } } };
    send(get, node_version->value);

    const auto payload = receive(transaction::command);
    const auto message = transaction::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->transaction_ptr->get_hash(true) == genesis_coinbase());
}

BOOST_AUTO_TEST_SUITE_END()
