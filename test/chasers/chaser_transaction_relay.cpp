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
#include "chaser_transaction_setup_fixture.hpp"

BC_AUTO_TEST_SUITE(chaser_transaction_relay_tests)

// relay

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__all_reserved__none, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 0u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 1u);
    reserve_relays();
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), node::relay_slots);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__tiers__lowest_outside_reserves_above, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::inbound), 5u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 2u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__inbound_exhausted__reserves_free, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::inbound), 59u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::inbound), node::relay_slots);
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::outbound), 3u);
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::manual), 2u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__outbound_reserve_full__inbound_slot, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 2u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 3u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 4u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 5u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::inbound), 6u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__manual_reserve_full__outbound_slot, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 0u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 1u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 2u);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::outbound), 3u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reserve_relay__outbound_exhausted__manual_reserve_free, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::outbound), 62u);
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::manual), 2u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__release_relay__inbound__reserved_again, chaser_transaction_reserves_setup_fixture)
{
    BC_REQUIRE_EQUAL(reserve_relays(node::relay_tier::inbound), 59u);
    node_.release_relay(7);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::inbound), 7u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__no_slot__not_relayed, chaser_transaction_pooling_setup_fixture)
{
    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ spend(parent_value) })).first, node::error::success);
    BC_REQUIRE(recorded(events).empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__reserved_slot_not_started__not_relayed, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE_EQUAL(node_.reserve_relay(node::relay_tier::manual), 0u);

    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ spend(parent_value) })).first, node::error::success);
    BC_REQUIRE(recorded(events).empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__two_slots__relayed_to_both, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const relayed expected{ { tx->hash(false), 0b11 } };
    BC_REQUIRE_EQUAL(reserve_relaying(), 0u);
    BC_REQUIRE_EQUAL(reserve_relaying(), 1u);

    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(recorded(events) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__test__not_relayed, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE_EQUAL(reserve_relaying(), 0u);

    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ spend(parent_value) }), true).first, node::error::success);
    BC_REQUIRE(recorded(events).empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__origin__not_relayed_to_origin, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const relayed expected{ { tx->hash(false), 0b10 } };
    BC_REQUIRE_EQUAL(reserve_relaying(), 0u);
    BC_REQUIRE_EQUAL(reserve_relaying(), 1u);

    const auto events = record();
    BC_REQUIRE_EQUAL(submit_announced(package({ tx }), 0).first, node::error::success);
    BC_REQUIRE(recorded(events) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__below_rate__not_relayed, chaser_transaction_pooling_setup_fixture)
{
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);

    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ spend(parent_value - 1'000) })).first, node::error::success);
    BC_REQUIRE(recorded(events).empty());
}

// The parent (1,000 satoshis on 60 vbytes) is below the rate, the child
// (20,000) clears it with the parent.
BC_FIXTURE_TEST_CASE(chaser_transaction__relay__child_pays_for_parent__both_in_order, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const auto high = spend(low->hash(false), parent_value - 21'000);
    const relayed expected{ { low->hash(false), 0b01 }, { high->hash(false), 0b01 } };
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);

    const auto first = record();
    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);
    BC_REQUIRE(recorded(first).empty());

    const auto second = record();
    BC_REQUIRE_EQUAL(submit(package({ high })).first, node::error::success);
    BC_REQUIRE(recorded(second) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__parent_held__child_alone, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const auto high = spend(low->hash(false), parent_value - 21'000);
    const relayed expected{ { high->hash(false), 0b01 } };
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);
    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);
    node_.set_relayed(slot, { query_.to_tx(low->hash(false)) });

    const auto events = record();
    BC_REQUIRE_EQUAL(submit(package({ high })).first, node::error::success);
    BC_REQUIRE(recorded(events) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__rate_fall__swept, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const relayed expected{ { low->hash(false), 0b01 } };
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);
    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);

    const auto events = record();
    node_.set_relay_rate(slot, 10'000);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(events) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__relay__rate_rise__not_swept, chaser_transaction_pooling_setup_fixture)
{
    const auto low = spend(parent_value - 1'000);
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);
    BC_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);

    const auto events = record();
    node_.set_relay_rate(slot, 200'000);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(events).empty());
}

// The slot is swept once between blocks, and its later fall after the next.
BC_FIXTURE_TEST_CASE(chaser_transaction__relay__second_rate_fall__swept_after_block, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 2'000);
    const relayed expected{ { second->hash(false), 0b01 } };
    const auto slot = reserve_relaying();
    node_.set_relay_rate(slot, 100'000);
    BC_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);
    node_.set_relay_rate(slot, 10'000);
    node_.set_relay_rate(slot, 100'000);
    BC_REQUIRE_EQUAL(submit(package({ second })).first, node::error::success);

    const auto deferred = record();
    node_.set_relay_rate(slot, 10'000);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(deferred).empty());

    const auto swept = record();
    BC_REQUIRE(!confirm().is_terminal());
    BC_REQUIRE(bumped(3));
    BC_REQUIRE(recorded(swept) == expected);
}

// The sweep requested of a reserved slot is made when relay to it starts.
BC_FIXTURE_TEST_CASE(chaser_transaction__relay_pool__slot_not_started__swept_when_started, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const relayed expected{ { tx->hash(false), 0b01 } };
    const auto slot = node_.reserve_relay(node::relay_tier::manual);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto owed = record();
    node_.relay_pool(slot);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(owed).empty());

    const auto started = record();
    node_.activate_relay(slot);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(started) == expected);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__release_relay__reserved_again__marks_cleared, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const relayed expected{ { tx->hash(false), 0b01 } };
    BC_REQUIRE_EQUAL(reserve_relaying(), 0u);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    node_.release_relay(0);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE_EQUAL(reserve_relaying(), 0u);

    const auto events = record();
    node_.set_relay_rate(0, 1);
    node_.set_relay_rate(0, 0);
    BC_REQUIRE(!get_confirmable_txs().first);
    BC_REQUIRE(recorded(events) == expected);
}

BC_AUTO_TEST_SUITE_END()
