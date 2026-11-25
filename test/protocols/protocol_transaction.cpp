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
#include "../chasers/chaser_transaction_setup_fixture.hpp"

BC_AUTO_TEST_SUITE(protocol_transaction_tests)

// protocol_transaction_in

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__unknown_tx__get_data, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);

    const auto payload = receive(peer::get_data::command);
    const auto message = peer::get_data::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().is_transaction_type());
    BC_REQUIRE(message->items.front().hash == one_hash);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__pooled_tx__no_get_data, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__stored_unpooled_tx__get_data, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    BC_REQUIRE(query_.set(*tx));
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__not_current__no_get_data, chaser_transaction_relay_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__unrequested__stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::transaction{ spend(parent_value) }, node_version->value);
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__unrequested__stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__requested__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__requested__tx_unserved_fired, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
    BC_REQUIRE_EQUAL(fired_value(events::tx_unserved, [&]() { send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value); }), one);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__stopping__requested__tx_unserved_fired, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash }, { type_id::transaction, missing_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
    BC_REQUIRE_EQUAL(fired_value(events::tx_unserved, [&]() { disconnect(); }), 2u);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_missing_prevout__tx_refused_fired, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto missing = spend(missing_hash, parent_value);
    send(peer::inventory{ { { type_id::transaction, missing->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
    BC_REQUIRE_EQUAL(fired_value(events::tx_refused, [&]() { send(peer::transaction{ missing }, node_version->value); }), one);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_valid__archived, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BC_REQUIRE(archived(tx->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_missing_prevout__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto missing = spend(missing_hash, parent_value);
    const auto valid = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, missing->hash(false) }, { type_id::transaction, valid->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ missing }, node_version->value);
    send(peer::transaction{ valid }, node_version->value);
    BC_REQUIRE(archived(valid->hash(false)));
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_overspent__stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(add1(parent_value));
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__block_only__no_get_data, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::block, one_hash } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__over_maximum_inventory__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(peer::max_inventory, 1), node_version->value);
    send(announcement(1, 2), node_version->value);
    send(announcement(1, 3), node_version->value);
    BC_REQUIRE(received_before_pong(peer::pong::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__maximum_requests__get_data, chaser_transaction_requests_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(2, 1), node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__over_maximum_requests__stopped, chaser_transaction_requests_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(3, 1), node_version->value);
    BC_REQUIRE(dropped(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__over_maximum_requests_unanswered__stopped, chaser_transaction_requests_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(2, 1), node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(announcement(1, 2), node_version->value);
    BC_REQUIRE(dropped(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__maximum_requests_answered__get_data, chaser_transaction_requests_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(2, 1), node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::not_found{ announcement(2, 1).items }, node_version->value);
    send(announcement(2, 2), node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__request__unanswered__stopped, chaser_transaction_request_timeout_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__request__later_answered_oldest_unanswered__stopped, chaser_transaction_request_timeout_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__block_only__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::not_found{ { { type_id::block, one_hash } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_insufficient_fee_70013__not_stopped, chaser_transaction_fee_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto low = spend(parent_value);
    const auto high = spend(parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, low->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ low }, node_version->value);
    send(peer::inventory{ { { type_id::transaction, high->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ high }, node_version->value);
    BC_REQUIRE(archived(high->hash(false)));
    BC_REQUIRE(!query_.is_tx(low->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_insufficient_fee_70001__not_stopped, chaser_transaction_fee_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::bip37, true));

    const auto low = spend(parent_value);
    const auto high = spend(parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, low->hash(false) } } }, peer::level::bip37);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ low }, peer::level::bip37);
    send(peer::inventory{ { { type_id::transaction, high->hash(false) } } }, peer::level::bip37);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ high }, peer::level::bip37);
    BC_REQUIRE(archived(high->hash(false)));
    BC_REQUIRE(!query_.is_tx(low->hash(false)));
}

// sets

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__unpaid_parent_paying_child__both_archived, chaser_transaction_fee_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto parent = spend(parent_value);
    const auto child = spend(parent->hash(false), parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, parent->hash(false) }, { type_id::transaction, child->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ parent }, node_version->value);
    send(peer::transaction{ child }, node_version->value);
    BC_REQUIRE(archived(child->hash(false)));
    BC_REQUIRE(query_.is_tx(parent->hash(false)));
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__unpaid_parent_child_not_found__not_archived_not_stopped, chaser_transaction_fee_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto parent = spend(parent_value);
    const auto child = spend(parent->hash(false), parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, parent->hash(false) }, { type_id::transaction, child->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ parent }, node_version->value);
    send(peer::not_found{ { { type_id::transaction, child->hash(false) } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
    BC_REQUIRE(!query_.is_tx(parent->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__one_not_found__other_archived, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) }, { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(archived(tx->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__tx_of_later_announcement__earlier_archived_as_it_stands, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    send(peer::inventory{ { { type_id::transaction, first->hash(false) }, { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::inventory{ { { type_id::transaction, second->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ first }, node_version->value);
    send(peer::transaction{ second }, node_version->value);
    BC_REQUIRE(archived(second->hash(false)));
    BC_REQUIRE(query_.is_tx(first->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__late_tx__archived_alone, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto late = spend(parent_value);
    const auto other = spend(missing_hash, parent_value);
    send(peer::inventory{ { { type_id::transaction, late->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::inventory{ { { type_id::transaction, other->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ other }, node_version->value);
    send(peer::transaction{ late }, node_version->value);
    BC_REQUIRE(archived(late->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__rivals__both_archived, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto first = spend(parent_value);
    const auto rival = spend(parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, first->hash(false) }, { type_id::transaction, rival->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ first }, node_version->value);
    send(peer::transaction{ rival }, node_version->value);
    BC_REQUIRE(archived(rival->hash(false)));
    BC_REQUIRE(query_.is_tx(first->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__child_of_orphan__neither_archived, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto orphan = spend(missing_hash, parent_value);
    const auto child = spend(orphan->hash(false), parent_value);
    const auto valid = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, orphan->hash(false) }, { type_id::transaction, child->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ orphan }, node_version->value);
    send(peer::transaction{ child }, node_version->value);
    send(peer::inventory{ { { type_id::transaction, valid->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ valid }, node_version->value);
    BC_REQUIRE(archived(valid->hash(false)));
    BC_REQUIRE(!query_.is_tx(orphan->hash(false)));
    BC_REQUIRE(!query_.is_tx(child->hash(false)));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__announcement__child_before_parent__parent_archived, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto parent = spend(parent_value);
    const auto child = spend(parent->hash(false), parent_value);
    send(peer::inventory{ { { type_id::transaction, child->hash(false) }, { type_id::transaction, parent->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ child }, node_version->value);
    send(peer::transaction{ parent }, node_version->value);
    BC_REQUIRE(archived(parent->hash(false)));
    BC_REQUIRE(!query_.is_tx(child->hash(false)));
}

// protocol_transaction_out

// distribution (tx in, acceptance, tx out)

BC_FIXTURE_TEST_CASE(protocol_transaction__distribution__peer_tx__accepted_announced_and_served, chaser_transaction_distribution_setup_fixture)
{
    const auto tx = spend(parent_value);
    const auto hash = tx->hash(false);
    const hashes expected{ hash };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto sink = connect();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    select(zero);
    BC_REQUIRE(relay(tx));
    BC_REQUIRE(archived(hash));
    BC_REQUIRE(pooled(query_.to_tx(hash)));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    select(sink);
    BC_REQUIRE_EQUAL(announced(1), expected);
    BC_REQUIRE_EQUAL(served(hash), hash);
}

BC_FIXTURE_TEST_CASE(protocol_transaction__distribution__peer_chain__announced_in_order, chaser_transaction_distribution_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto third = spend(second->hash(false), parent_value);
    const hashes expected{ first->hash(false), second->hash(false), third->hash(false) };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto sink = connect();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    select(zero);
    BC_REQUIRE(relay(first));
    BC_REQUIRE(archived(first->hash(false)));
    BC_REQUIRE(relay(second));
    BC_REQUIRE(archived(second->hash(false)));
    BC_REQUIRE(relay(third));
    BC_REQUIRE(archived(third->hash(false)));

    select(sink);
    BC_REQUIRE_EQUAL(announced(3), expected);
    BC_REQUIRE_EQUAL(served(third->hash(false)), third->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction__distribution__peer_conflict__both_accepted_and_announced, chaser_transaction_distribution_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(sub1(parent_value));
    const hashes expected{ first->hash(false), second->hash(false) };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto sink = connect();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    select(zero);
    BC_REQUIRE(relay(first));
    BC_REQUIRE(archived(first->hash(false)));
    BC_REQUIRE(relay(second));
    BC_REQUIRE(archived(second->hash(false)));

    select(sink);
    BC_REQUIRE_EQUAL(announced(2), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction__distribution__submitted_tx__announced_to_every_peer, chaser_transaction_distribution_setup_fixture)
{
    const auto tx = spend(parent_value);
    const hashes expected{ tx->hash(false) };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    const auto other = connect();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    select(zero);
    BC_REQUIRE_EQUAL(announced(1), expected);

    select(other);
    BC_REQUIRE_EQUAL(announced(1), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__start__minimum_fee_rate__zero_fee_filter, chaser_transaction_fee_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto payload = receive(peer::fee_filter::command);
    const auto message = peer::fee_filter::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->minimum_fee, 0u);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__start__zero_minimum_fee_rate__zero_fee_filter, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto payload = receive(peer::fee_filter::command);
    const auto message = peer::fee_filter::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->minimum_fee, 0u);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__submit__archived__inventory, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().type == type_id::transaction);
    BC_REQUIRE(message->items.front().hash == tx->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__submit__child_pays_for_parent__both_announced, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const auto high = spend(low->hash(false), parent_value - 21'000);
    const hashes expected{ low->hash(false), high->hash(false) };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 100'000 }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);
    BC_REQUIRE_EQUAL(submit(package({ high })).first, node::error::success);
    BC_REQUIRE_EQUAL(announced(2), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__fee_filter__rate_fall__swept_inventory, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const hashes expected{ low->hash(false) };
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 100'000 }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));
    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);

    send(peer::fee_filter{ 10'000 }, node_version->value);
    BC_REQUIRE_EQUAL(announced(1), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__fee_filter__second_fall_within_period__dropped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    send(peer::fee_filter{ 100'000 }, node_version->value);
    send(peer::fee_filter{ 10'000 }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    send(peer::fee_filter{ 1'000 }, node_version->value);
    send(peer::ping{ 42 }, node_version->value);
    BC_REQUIRE(dropped(peer::pong::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__fee_filter__rises_after_fall__kept, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    send(peer::fee_filter{ 100'000 }, node_version->value);
    send(peer::fee_filter{ 10'000 }, node_version->value);
    send(peer::fee_filter{ 20'000 }, node_version->value);
    send(peer::fee_filter{ 30'000 }, node_version->value);
    send(peer::ping{ 42 }, node_version->value);
    BC_REQUIRE(!dropped(peer::pong::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__start__no_relay_slot__max_money_fee_filter, chaser_transaction_pooling_setup_fixture)
{
    reserve_relays();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto payload = receive(peer::fee_filter::command);
    const auto message = peer::fee_filter::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->minimum_fee, config_.bitcoin.max_money());
}

BC_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__no_relay_slot__no_get_data, chaser_transaction_pooling_setup_fixture)
{
    reserve_relays();
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!receive(peer::fee_filter::command).empty());

    send(peer::inventory{ { { type_id::transaction, spend(parent_value)->hash(false) } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__submit__announced_by_peer__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto first = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, first->hash(false) } } }, node_version->value);
    BC_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ first }, node_version->value);
    BC_REQUIRE(archived(first->hash(false)));

    const auto next = spend(first->hash(false), parent_value);
    BC_REQUIRE_EQUAL(submit(package({ next })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().hash == next->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__stored_tx__transaction, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::transaction, parent().hash(false) } } }, node_version->value);

    const auto payload = receive(peer::transaction::command);
    const auto message = peer::transaction::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE(message->transaction_ptr->hash(false) == parent().hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__witness_not_provided__stopped, chaser_transaction_no_witness_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::witness_tx, parent().hash(false) } } }, node_version->value);
    BC_REQUIRE(dropped(peer::transaction::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__unknown_tx_not_found_disabled__stopped, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::transaction, one_hash } } }, node_version->value);
    BC_REQUIRE(dropped(peer::not_found::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__block_then_stored_tx__transaction, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(peer::get_data{ { { type_id::block, genesis.hash() }, { type_id::transaction, parent().hash(false) } } }, node_version->value);

    const auto message = peer::transaction::deserialize(node_version->value, receive_before_pong(peer::transaction::command));
    BC_REQUIRE(message);
    BC_REQUIRE(message->transaction_ptr->hash(false) == parent().hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__relay_event__missing_tx_70001__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::bip37, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    node_.notify(node::error::success, chases::relay{ node::transaction_t{ 42 }, max_uint64 });

    // The announcement of a subsequent tx orders the missing tx event.
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().hash == tx->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__relay_event__missing_tx_70013__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    node_.notify(node::error::success, chases::relay{ node::transaction_t{ 42 }, max_uint64 });

    // The announcement of a subsequent tx orders the missing tx event.
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().hash == tx->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__stale_event__became_current__fee_filter, chaser_transaction_relay_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto initial = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BC_REQUIRE(initial);
    BC_REQUIRE_EQUAL(initial->minimum_fee, config_.bitcoin.max_money());

    config_.node.currency_window_minutes = 0;
    node_.notify(node::error::success, chases::stale{});

    const auto current = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(current->minimum_fee, 0u);
}

// memory_pool

BC_FIXTURE_TEST_CASE(protocol_transaction_out__request_mempool__inbound__not_requested, chaser_transaction_request_mempool_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::memory_pool::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__request_mempool__manual__requested_after_fee_filter, chaser_transaction_request_mempool_setup_fixture)
{
    accept();
    BC_REQUIRE(handshake(config_.node.services_required(), peer::level::maximum_protocol, true));

    const auto filter = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BC_REQUIRE(filter);
    BC_REQUIRE_EQUAL(filter->minimum_fee, 0u);
    BC_REQUIRE(received(peer::memory_pool::command, peer::pong::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__request_mempool__manual_disabled__not_requested, chaser_transaction_pooling_setup_fixture)
{
    accept();
    BC_REQUIRE(handshake(config_.node.services_required(), peer::level::maximum_protocol, true));

    const auto filter = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BC_REQUIRE(filter);
    BC_REQUIRE_EQUAL(filter->minimum_fee, 0u);
    BC_REQUIRE(!received(peer::memory_pool::command, peer::pong::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__disabled__no_inventory, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__empty__no_inventory, chaser_transaction_memory_pool_setup_fixture)
{
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__pooled__inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto parent_tx = spend(parent_value);
    const auto child_tx = spend(parent_tx->hash(false), parent_value);
    const hashes expected{ parent_tx->hash(false), child_tx->hash(false) };
    BC_REQUIRE_EQUAL(submit(package({ parent_tx, child_tx })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE_EQUAL(announced(2), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__bip35__inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::bip35, true));

    send(peer::memory_pool{}, peer::level::bip35);

    const auto message = peer::inventory::deserialize(peer::level::bip35, receive(peer::inventory::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().type == type_id::transaction);
    BC_REQUIRE(message->items.front().hash == tx->hash(false));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__below_fee_filter__no_inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 1'000 }, node_version->value);
    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__child_pays_for_parent__both_inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const auto high = spend(low->hash(false), parent_value - 21'000);
    const hashes expected{ low->hash(false), high->hash(false) };
    BC_REQUIRE_EQUAL(submit(package({ low, high })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 100'000 }, node_version->value);
    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE_EQUAL(announced(2), expected);
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__relayed__not_announced_again, chaser_transaction_memory_pool_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const hashes expected{ second->hash(false) };
    BC_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BC_REQUIRE(!received_before_pong(peer::inventory::command));

    send(peer::inventory{ { { type_id::transaction, first->hash(false) } } }, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::get_data::command));
    BC_REQUIRE_EQUAL(submit(package({ second })).first, node::error::success);
    BC_REQUIRE_EQUAL(announced(1), expected);

    send(peer::memory_pool{}, node_version->value);
    BC_REQUIRE(!received_before_pong(peer::inventory::command));
}

BC_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__witness_relay__wtxid_inventory, chaser_transaction_witness_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(handshake(0, peer::level::bip339, true, true));

    send(peer::memory_pool{}, node_version->value);

    const auto message = peer::inventory::deserialize(node_version->value, receive(peer::inventory::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->items.size(), one);
    BC_REQUIRE(message->items.front().type == type_id::wtxid);
    BC_REQUIRE(message->items.front().hash == tx->hash(true));
}

BC_AUTO_TEST_SUITE_END()
