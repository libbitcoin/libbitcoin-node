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

BC_FIXTURE_TEST_SUITE(functional_p2p_compact_tests, p2p_compact_setup_fixture)

using namespace system;
using namespace network::messages::peer;

constexpr uint64_t full_node = service::node_network | service::node_witness;

static compact_block prefilled(const chain::block& block,
    const chain::transaction::cptr& coinbase)
{
    return { block.header_ptr(), 42, {}, { { 0, coinbase } } };
}

static compact_block shortened(const chain::block& block)
{
    const auto key = chain::short_id::to_key(block.header(), 42);
    const auto& coinbase = *block.transactions_ptr()->front();
    const auto hash = bitcoin_hash(coinbase.to_data(true));
    const auto integer = chain::short_id::to_id(key, hash);
    const auto id = chain::short_id::to_mini(integer);
    return { block.header_ptr(), 42, { id }, {} };
}

// sendcmpct
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(functional_p2p_compact__handshake__verack__send_compact_low_bandwidth)
{
    BC_REQUIRE(handshake());

    const auto message = send_compact::deserialize(node_version->value, receive(send_compact::command));
    BC_REQUIRE(message);
    BC_REQUIRE(!message->high_bandwidth);
    BC_REQUIRE_EQUAL(message->compact_version, send_compact::compact_version_2);
}

// Headers completion when current requests announcement of a signaled peer.
BC_AUTO_TEST_CASE(functional_p2p_compact__headers__complete__send_compact_high_bandwidth)
{
    BC_REQUIRE(handshake(full_node));
    send(send_compact{ false, send_compact::compact_version_2 }, node_version->value);

    receive(get_headers::command);
    send(headers{}, node_version->value);

    const auto message = send_compact::deserialize(node_version->value, receive(send_compact::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->high_bandwidth);
    BC_REQUIRE_EQUAL(message->compact_version, send_compact::compact_version_2);
}

// A peer that has not signaled compact blocks is not asked to announce.
BC_AUTO_TEST_CASE(functional_p2p_compact__headers__complete_unsignaled__no_high_bandwidth)
{
    BC_REQUIRE(handshake(full_node));
    receive(send_compact::command);

    receive(get_headers::command);
    send(headers{}, node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(!received(send_compact::command, pong::command));
}

// A peer signaling after headers completion is then asked to announce.
BC_AUTO_TEST_CASE(functional_p2p_compact__send_compact__after_complete__send_compact_high_bandwidth)
{
    BC_REQUIRE(handshake(full_node));
    receive(send_compact::command);

    receive(get_headers::command);
    send(headers{}, node_version->value);
    send(send_compact{ false, send_compact::compact_version_2 }, node_version->value);

    const auto message = send_compact::deserialize(node_version->value, receive(send_compact::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->high_bandwidth);
    BC_REQUIRE_EQUAL(message->compact_version, send_compact::compact_version_2);
}

// cmpctblock (out)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(functional_p2p_compact__get_data__genesis__prefilled_coinbase)
{
    BC_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::compact, genesis.hash() } } };
    send(get, node_version->value);

    const auto message = compact_block::deserialize(node_version->value, receive(compact_block::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->header_ptr->hash() == genesis.hash());
    BC_REQUIRE(message->short_ids.empty());
    BC_REQUIRE_EQUAL(message->transactions.size(), one);
    BC_REQUIRE_EQUAL(message->transactions.front().index, zero);
    BC_REQUIRE(message->transactions.front().transaction_ptr->hash(false) == genesis.transactions_ptr()->front()->hash(false));
}

// blocktxn (out)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(functional_p2p_compact__get_compact_transactions__genesis_coinbase__expected)
{
    BC_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_compact_transactions{ genesis.hash(), { 0 } }, node_version->value);

    const auto message = compact_transactions::deserialize(node_version->value, receive(compact_transactions::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->block_hash == genesis.hash());
    BC_REQUIRE_EQUAL(message->transaction_ptrs.size(), one);
    BC_REQUIRE(message->transaction_ptrs.front()->hash(false) == genesis.transactions_ptr()->front()->hash(false));
}

BC_AUTO_TEST_CASE(functional_p2p_compact__get_compact_transactions__out_of_range__stopped)
{
    BC_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_compact_transactions{ genesis.hash(), { 1 } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(dropped(pong::command));
}

// cmpctblock (in)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(functional_p2p_compact__compact_block__prefilled__associated)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(prefilled(block, block.transactions_ptr()->front()), node_version->value);

    BC_REQUIRE(associated(block.hash()));
}

BC_AUTO_TEST_CASE(functional_p2p_compact__compact_block__short_id__requested_and_associated)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);

    const auto request = get_compact_transactions::deserialize(node_version->value, receive(get_compact_transactions::command));
    BC_REQUIRE(request);
    BC_REQUIRE(request->block_hash == block.hash());
    BC_REQUIRE_EQUAL(request->indexes.size(), one);
    BC_REQUIRE_EQUAL(request->indexes.front(), 0u);

    send(compact_transactions{ block.hash(), { block.transactions_ptr()->front() } }, node_version->value);
    BC_REQUIRE(associated(block.hash()));
}

BC_AUTO_TEST_CASE(functional_p2p_compact__compact_block__replayed_while_organizing__requested_once)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);
    send(shortened(block), node_version->value);
    send(ping{ 42 }, node_version->value);

    receive(get_compact_transactions::command);
    BC_REQUIRE(!received(get_compact_transactions::command, pong::command));
}

// Block 2 remains an unassociated candidate, so the chain never coalesces.
BC_FIXTURE_TEST_CASE(functional_p2p_compact__compact_block__replayed_while_submitting__not_requested, p2p_compact_candidate_setup_fixture)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);
    receive(get_compact_transactions::command);

    send(compact_transactions{ block.hash(), { block.transactions_ptr()->front() } }, node_version->value);
    send(shortened(block), node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(!received(get_compact_transactions::command, pong::command));
    BC_REQUIRE(associated(block.hash()));
}

// An unstored parent is resolved by header synchronization.
BC_AUTO_TEST_CASE(functional_p2p_compact__compact_block__orphan__not_stopped)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block2();
    send(prefilled(block, block.transactions_ptr()->front()), node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(!receive(pong::command).empty());
}

// The pool holds none of the block, so it is downloaded (not requested).
BC_FIXTURE_TEST_CASE(functional_p2p_compact__compact_block__short_id_unpooled__not_requested, p2p_compact_pooled_setup_fixture)
{
    BC_REQUIRE(handshake(full_node));

    send(shortened(block1()), node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(!received(get_compact_transactions::command, pong::command));
}

BC_AUTO_TEST_CASE(functional_p2p_compact__compact_block__short_id_mismatch__stopped)
{
    BC_REQUIRE(handshake(full_node));

    const auto& block = block1();
    send(shortened(block), node_version->value);
    receive(get_compact_transactions::command);

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(compact_transactions{ block.hash(), { genesis.transactions_ptr()->front() } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BC_REQUIRE(dropped(pong::command));
}

// An unidentified fill leaves evidence, resolved against the archived block.
// Block 2 remains an unassociated candidate, so the chain never coalesces
// (which would prune the store, suspending the network).
BC_FIXTURE_TEST_CASE(functional_p2p_compact__compact_block__unidentified_then_archived__stopped, p2p_compact_candidate_setup_fixture)
{
    BC_REQUIRE(handshake(full_node));

    // The unassociated candidates are requested on handshake.
    const auto& block = block1();
    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BC_REQUIRE(request);
    BC_REQUIRE(request->items.front().hash == block.hash());

    // The fill is unidentified (wrong coinbase), the pong orders it.
    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(prefilled(block, genesis.transactions_ptr()->front()), node_version->value);
    send(ping{ 1 }, node_version->value);
    receive(pong::command);

    send(network::messages::peer::block::command, block.to_data(true));
    BC_REQUIRE(associated(block.hash()));

    send(prefilled(block, block.transactions_ptr()->front()), node_version->value);
    send(ping{ 2 }, node_version->value);

    BC_REQUIRE(dropped(pong::command));
}

// cmpctblock (announce)
// ----------------------------------------------------------------------------

// High bandwidth announcement does not require sendheaders.
BC_FIXTURE_TEST_CASE(functional_p2p_compact__send_compact__high_bandwidth__compact_block_announced, p2p_compact_candidate_setup_fixture)
{
    BC_REQUIRE(handshake(full_node));
    send(send_compact{ true, send_compact::compact_version_2 }, node_version->value);

    const auto& block = block1();
    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BC_REQUIRE(request);
    BC_REQUIRE(request->items.front().hash == block.hash());

    send(network::messages::peer::block::command, block.to_data(true));

    const auto message = compact_block::deserialize(node_version->value, receive(compact_block::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->header_ptr->hash() == block.hash());
}

BC_AUTO_TEST_SUITE_END()
