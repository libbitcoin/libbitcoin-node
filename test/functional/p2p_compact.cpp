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

BOOST_FIXTURE_TEST_SUITE(functional_p2p_compact_tests, p2p_compact_setup_fixture)

using namespace system;
using namespace network::messages::peer;

constexpr uint64_t full_node = service::node_network | service::node_witness;

struct compact_accessor
  : node::protocol_peer
{
    using protocol_peer::to_compact_key;
    using protocol_peer::to_short_id;
    using protocol_peer::to_mini;
};

static compact_block prefilled(const chain::block& block, const chain::transaction::cptr& coinbase)
{
    return { block.header_ptr(), 42, {}, { { 0, coinbase } } };
}

static compact_block shortened(const chain::block& block)
{
    const auto key = compact_accessor::to_compact_key(block.header(), 42);
    const auto& coinbase = *block.transactions_ptr()->front();
    const auto id = compact_accessor::to_mini(compact_accessor::to_short_id(key, coinbase.hash(true)));
    return { block.header_ptr(), 42, { id }, {} };
}

// sendcmpct
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p_compact__handshake__verack__send_compact_low_bandwidth)
{
    BOOST_REQUIRE(handshake());

    const auto message = send_compact::deserialize(node_version->value, receive(send_compact::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(!message->high_bandwidth);
    BOOST_REQUIRE_EQUAL(message->compact_version, send_compact::compact_version_2);
}

// Headers completion when current requests compact block announcement.
BOOST_AUTO_TEST_CASE(functional_p2p_compact__headers__complete__send_compact_high_bandwidth)
{
    BOOST_REQUIRE(handshake(full_node));

    receive(get_headers::command);
    send(headers{}, node_version->value);

    const auto message = send_compact::deserialize(node_version->value, receive(send_compact::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->high_bandwidth);
    BOOST_REQUIRE_EQUAL(message->compact_version, send_compact::compact_version_2);
}

// cmpctblock (out)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p_compact__get_data__genesis__prefilled_coinbase)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::compact, genesis.hash() } } };
    send(get, node_version->value);

    const auto message = compact_block::deserialize(node_version->value, receive(compact_block::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->header_ptr->hash() == genesis.hash());
    BOOST_REQUIRE(message->short_ids.empty());
    BOOST_REQUIRE_EQUAL(message->transactions.size(), one);
    BOOST_REQUIRE_EQUAL(message->transactions.front().index, zero);
    BOOST_REQUIRE(message->transactions.front().transaction_ptr->hash(false) == genesis.transactions_ptr()->front()->hash(false));
}

// blocktxn (out)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p_compact__get_compact_transactions__genesis_coinbase__expected)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_compact_transactions{ genesis.hash(), { 0 } }, node_version->value);

    const auto message = compact_transactions::deserialize(node_version->value, receive(compact_transactions::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->block_hash == genesis.hash());
    BOOST_REQUIRE_EQUAL(message->transaction_ptrs.size(), one);
    BOOST_REQUIRE(message->transaction_ptrs.front()->hash(false) == genesis.transactions_ptr()->front()->hash(false));
}

BOOST_AUTO_TEST_CASE(functional_p2p_compact__get_compact_transactions__out_of_range__stopped)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_compact_transactions{ genesis.hash(), { 1 } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

// cmpctblock (in)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p_compact__compact_block__prefilled__associated)
{
    BOOST_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(prefilled(block, block.transactions_ptr()->front()), node_version->value);

    BOOST_REQUIRE(associated(block.hash()));
}

BOOST_AUTO_TEST_CASE(functional_p2p_compact__compact_block__short_id__requested_and_associated)
{
    BOOST_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);

    const auto request = get_compact_transactions::deserialize(node_version->value, receive(get_compact_transactions::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE(request->block_hash == block.hash());
    BOOST_REQUIRE_EQUAL(request->indexes.size(), one);
    BOOST_REQUIRE_EQUAL(request->indexes.front(), 0u);

    send(compact_transactions{ block.hash(), { block.transactions_ptr()->front() } }, node_version->value);
    BOOST_REQUIRE(associated(block.hash()));
}

BOOST_AUTO_TEST_CASE(functional_p2p_compact__compact_block__short_id_mismatch__stopped)
{
    BOOST_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);
    receive(get_compact_transactions::command);

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(compact_transactions{ block.hash(), { genesis.transactions_ptr()->front() } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

// An unidentified fill leaves evidence, resolved against the archived block.
BOOST_AUTO_TEST_CASE(functional_p2p_compact__compact_block__unidentified_then_archived__stopped)
{
    BOOST_REQUIRE(handshake(full_node));

    const auto& block = block1();
    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(prefilled(block, genesis.transactions_ptr()->front()), node_version->value);
    BOOST_REQUIRE(await([&]() { return query_.get_top_candidate() == one; }));

    // Headers are proven, then requested again for archival. The deferred
    // compact top is then downloaded, as it is no longer the top.
    const headers announcement{ { block2().header_ptr() } };
    send(announcement, node_version->value);
    receive(get_headers::command);
    receive(get_headers::command);
    send(announcement, node_version->value);
    BOOST_REQUIRE(await([&]() { return query_.get_top_candidate() == two; }));

    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE(request->items.front().hash == block.hash());

    send(network::messages::peer::block::command, block.to_data(true));
    BOOST_REQUIRE(associated(block.hash()));

    send(prefilled(block, block.transactions_ptr()->front()), node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

// cmpctblock (announce)
// ----------------------------------------------------------------------------

// High bandwidth announcement does not require sendheaders.
BOOST_FIXTURE_TEST_CASE(functional_p2p_compact__send_compact__high_bandwidth__compact_block_announced, p2p_compact_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(full_node));
    send(send_compact{ true, send_compact::compact_version_2 }, node_version->value);

    const auto& block = block1();
    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE(request->items.front().hash == block.hash());

    send(network::messages::peer::block::command, block.to_data(true));

    const auto message = compact_block::deserialize(node_version->value, receive(compact_block::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->header_ptr->hash() == block.hash());
}

BOOST_AUTO_TEST_SUITE_END()
