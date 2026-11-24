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

BC_AUTO_TEST_SUITE(chaser_transaction_submit_tests)

// submit

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__relay_disabled__pooling_disabled, chaser_transaction_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BC_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__not_current__pooling_disabled, chaser_transaction_relay_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BC_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__empty__empty_package, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({}));
    BC_REQUIRE_EQUAL(result.first, node::error::empty_package);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__stored_unpooled__pooled, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE(query_.set(*tx));

    const auto stored = query_.to_tx(tx->hash(false));
    BC_REQUIRE(!query_.is_pooled(stored));

    const auto result = submit(package({ tx }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_pooled(stored));
    BC_REQUIRE_EQUAL(query_.to_tx(tx->hash(false)), stored);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__confirmed__duplicate_transaction, chaser_transaction_single_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto block = confirm(tx);
    const auto result = submit(package({ tx }));
    BC_REQUIRE(!block.is_terminal());
    BC_REQUIRE_EQUAL(result.first, node::error::duplicate_transaction);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__forward_reference__forward_reference, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto result = submit(package({ second, first }));
    BC_REQUIRE_EQUAL(result.first, system::error::forward_reference);
    BC_REQUIRE(!query_.is_tx(first->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__internal_double_spend__block_internal_double_spend, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value), spend(sub1(parent_value)) }));
    BC_REQUIRE_EQUAL(result.first, system::error::block_internal_double_spend);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__coinbase__coinbase_transaction, chaser_transaction_pooling_setup_fixture)
{
    const chain::block& genesis = config_.bitcoin.genesis_block;
    const auto result = submit(package({ genesis.transactions_ptr()->front() }));
    BC_REQUIRE_EQUAL(result.first, system::error::coinbase_transaction);
    BC_REQUIRE_EQUAL(result.second, zero);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__empty_transaction__empty_transaction, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ std::make_shared<const chain::transaction>() }));
    BC_REQUIRE_EQUAL(result.first, system::error::empty_transaction);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__missing_prevout__missing_previous_output, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(missing_hash, parent_value) }));
    BC_REQUIRE_EQUAL(result.first, system::error::missing_previous_output);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__overspent__spend_exceeds_value, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(add1(parent_value)) }));
    BC_REQUIRE_EQUAL(result.first, system::error::spend_exceeds_value);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__second_overspent__index_one, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), add1(parent_value));
    const auto result = submit(package({ first, second }));
    BC_REQUIRE_EQUAL(result.first, system::error::spend_exceeds_value);
    BC_REQUIRE_EQUAL(result.second, one);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__absolute_locked__absolute_time_locked, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(parent().hash(false), parent_value, 1000, 0) }));
    BC_REQUIRE_EQUAL(result.first, system::error::absolute_time_locked);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__valid_test__success_not_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const auto result = submit(package({ tx }), true);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(!query_.is_tx(tx->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__valid__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const auto result = submit(package({ tx }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_tx(tx->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__wire_tx_demanded__broadcast, chaser_transaction_pooling_setup_fixture)
{
    std::promise<network::wire_tx::cptr> promise{};
    auto broadcast = promise.get_future();
    const auto session = std::make_shared<wire_session>(node_, 42);
    session->subscribe<network::wire_tx>([&](const code&, const network::wire_tx::cptr& message, uint64_t) NOEXCEPT
    {
        promise.set_value(message);
        return false;
    }, 42);

    const auto tx = spend(parent_value);
    node_.demand_wire_tx(true);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE(broadcast.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    BC_REQUIRE(broadcast.get()->tx()->to_data(true) == tx->to_data(true));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__valid_twice__duplicate_transaction, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BC_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::duplicate_transaction);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_result = submit(package({ first }));
    const auto second_result = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE(query_.is_tx(first->hash(false)));
    BC_REQUIRE(query_.is_tx(second->hash(false)));
}

// The best outcome (100 satoshis) does not cover both rivals at the rate.
BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict_uncovered__insufficient_fee, chaser_transaction_fee_setup_fixture)
{
    const auto first = submit(package({ spend(parent_value - 100) }));
    const auto second = submit(package({ spend(parent_value - 99) }));
    BC_REQUIRE_EQUAL(first.first, node::error::success);
    BC_REQUIRE_EQUAL(second.first, node::error::insufficient_fee);
    BC_REQUIRE_EQUAL(second.second, zero);
}

// The best outcome (1,000 satoshis) covers both rivals at the rate.
BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict_no_added_fee__covered_archived, chaser_transaction_fee_setup_fixture)
{
    const auto second = spend(parent_value - 999);
    const auto first_result = submit(package({ spend(parent_value - 1'000) }));
    const auto second_result = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE(query_.is_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict_added_fee__success_archived, chaser_transaction_fee_setup_fixture)
{
    const auto second = spend(parent_value - 2'000);
    const auto first_result = submit(package({ spend(parent_value - 1'000) }));
    const auto second_result = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE(query_.is_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict_outcomes_over_limit__success, chaser_transaction_limit_setup_fixture)
{
    const auto first = submit(package({ spend(parent_value) }));
    const auto second = submit(package({ spend(sub1(parent_value)) }));
    BC_REQUIRE_EQUAL(first.first, node::error::success);
    BC_REQUIRE_EQUAL(second.first, node::error::success);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__dominant_rival_over_outcome_limit__templated, chaser_transaction_limit_setup_fixture)
{
    const auto low = spend(parent_value);
    const auto high = spend(parent_value - 50'000);
    const auto first = submit(package({ low }));
    const auto second = submit(package({ high }));
    const auto built = build_template();
    const auto& links = std::get<2>(built);

    BC_REQUIRE_EQUAL(first.first, node::error::success);
    BC_REQUIRE_EQUAL(second.first, node::error::success);
    BC_REQUIRE_EQUAL(links.size(), one);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(high->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict_over_block__pool_limit, chaser_transaction_single_setup_fixture)
{
    const auto first = submit(package({ spend(parent_value - 1'000) }));
    const auto second = submit(package({ spend(parent_value - 2'000) }));
    BC_REQUIRE_EQUAL(first.first, node::error::success);
    BC_REQUIRE_EQUAL(second.first, database::error::pool_limit);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_announced__conflict_outcomes_over_limit__both_stored, chaser_transaction_limit_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto conflict = spend(sub1(parent_value));
    const auto child = spend(first->hash(false), parent_value);
    BC_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);

    const auto result = submit_announced(package({ conflict, child }), node_.reserve_relay(node::relay_tier::manual));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_tx(conflict->hash(false)));
    BC_REQUIRE(query_.is_tx(child->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_announced__conflict_over_block__passed_over, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto conflict = spend(parent_value - 2'000);
    const auto child = spend(first->hash(false), parent_value - 2'000);
    BC_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);

    const auto result = submit_announced(package({ conflict, child }), node_.reserve_relay(node::relay_tier::manual));
    BC_REQUIRE_EQUAL(result.first, database::error::pool_limit);
    BC_REQUIRE_EQUAL(result.second, zero);
    BC_REQUIRE(!query_.is_tx(conflict->hash(false)));
    BC_REQUIRE(query_.is_tx(child->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__over_reserved_sigops__pool_limit, chaser_transaction_sigops_setup_fixture)
{
    const auto result = submit(package({ spend_signed(parent_value - 1'000) }));
    BC_REQUIRE_EQUAL(result.first, database::error::pool_limit);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__no_sigops_reserved_sigops__success, chaser_transaction_sigops_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__over_block__pool_limit_not_archived, chaser_transaction_reserve_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto result = submit(package({ tx }));
    BC_REQUIRE_EQUAL(result.first, database::error::pool_limit);
    BC_REQUIRE_EQUAL(result.second, zero);
    BC_REQUIRE(!query_.is_tx(tx->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__parent_and_child__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    BC_REQUIRE_EQUAL(submit(package({ first, second })).first, node::error::success);
    BC_REQUIRE(query_.is_tx(first->hash(false)));
    BC_REQUIRE(query_.is_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__stored_parent_and_child__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    BC_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);
    BC_REQUIRE_EQUAL(submit(package({ first, second })).first, node::error::success);
    BC_REQUIRE(query_.is_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__unpooled_unconfirmed_parent__missing_previous_output, chaser_transaction_pooling_setup_fixture)
{
    const auto stored = spend(parent_value - 1'000);
    const auto child = spend(stored->hash(false), parent_value - 2'000);
    BC_REQUIRE(query_.set(*stored));

    const auto result = submit(package({ child }));
    BC_REQUIRE_EQUAL(result.first, system::error::missing_previous_output);
    BC_REQUIRE_EQUAL(result.second, zero);
    BC_REQUIRE(!query_.is_pooled(query_.to_tx(child->hash(false))));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_announced__unpooled_unconfirmed_parent__missing_previous_output, chaser_transaction_pooling_setup_fixture)
{
    const auto stored = spend(parent_value - 1'000);
    const auto child = spend(stored->hash(false), parent_value - 2'000);
    const auto other = spend(parent_value - 3'000);
    BC_REQUIRE(query_.set(*stored));

    const auto result = submit_announced(package({ child, other }), node_.reserve_relay(node::relay_tier::manual));
    BC_REQUIRE_EQUAL(result.first, system::error::missing_previous_output);
    BC_REQUIRE_EQUAL(result.second, zero);
    BC_REQUIRE(!query_.is_tx(child->hash(false)));
    BC_REQUIRE(query_.is_tx(other->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__zero_fee_minimum_rate__insufficient_fee, chaser_transaction_fee_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BC_REQUIRE_EQUAL(result.first, node::error::insufficient_fee);
    BC_REQUIRE_EQUAL(result.second, zero);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__sufficient_fee__success, chaser_transaction_fee_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
}

// prefixes

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__unpaid_parent_paying_child__both_archived, chaser_transaction_fee_setup_fixture)
{
    const auto parent = spend(parent_value);
    const auto child = spend(parent->hash(false), parent_value - 1'000);
    const auto result = submit(package({ parent, child }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_pooled(query_.to_tx(parent->hash(false))));
    BC_REQUIRE(query_.is_pooled(query_.to_tx(child->hash(false))));
}

// The paid tx's surplus does not cover the unpaid child.
BC_FIXTURE_TEST_CASE(chaser_transaction__submit__paid_then_unpaid__first_archived_fails_at_second, chaser_transaction_fee_setup_fixture)
{
    const auto paid = spend(parent_value - 100);
    const auto unpaid = spend(paid->hash(false), parent_value - 100);
    const auto result = submit(package({ paid, unpaid }));
    BC_REQUIRE_EQUAL(result.first, node::error::insufficient_fee);
    BC_REQUIRE_EQUAL(result.second, one);
    BC_REQUIRE(query_.is_pooled(query_.to_tx(paid->hash(false))));
    BC_REQUIRE(!query_.is_tx(unpaid->hash(false)));
}

// The paid tx's surplus covers the unpaid child (parent pays for child).
BC_FIXTURE_TEST_CASE(chaser_transaction__submit__paid_then_unpaid__parent_pays_for_child_both_archived, chaser_transaction_fee_setup_fixture)
{
    const auto paid = spend(parent_value - 1'000);
    const auto unpaid = spend(paid->hash(false), parent_value - 1'000);
    const auto first = submit(package({ paid }));
    const auto second = submit(package({ unpaid }));
    BC_REQUIRE_EQUAL(first.first, node::error::success);
    BC_REQUIRE_EQUAL(second.first, node::error::success);
    BC_REQUIRE(query_.is_pooled(query_.to_tx(unpaid->hash(false))));
}

// The paying child covers its parent but not also the unpaid grandchild.
BC_FIXTURE_TEST_CASE(chaser_transaction__submit__paying_child_then_unpaid__two_archived_fails_at_third, chaser_transaction_fee_setup_fixture)
{
    const auto parent = spend(parent_value);
    const auto child = spend(parent->hash(false), parent_value - 200);
    const auto unpaid = spend(child->hash(false), parent_value - 200);
    const auto result = submit(package({ parent, child, unpaid }));
    BC_REQUIRE_EQUAL(result.first, node::error::insufficient_fee);
    BC_REQUIRE_EQUAL(result.second, two);
    BC_REQUIRE(query_.is_pooled(query_.to_tx(parent->hash(false))));
    BC_REQUIRE(query_.is_pooled(query_.to_tx(child->hash(false))));
    BC_REQUIRE(!query_.is_tx(unpaid->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__paid_then_paid_child__both_archived_child_of_parent, chaser_transaction_fee_setup_fixture)
{
    const auto parent = spend(parent_value - 1'000);
    const auto child = spend(parent->hash(false), parent_value - 2'000);
    const auto result = submit(package({ parent, child }));
    const auto built = build_template();
    const auto& links = std::get<2>(built);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(links.size(), two);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(parent->hash(false)));
    BC_REQUIRE_EQUAL(links.back(), query_.to_tx(child->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__test_paid_then_unpaid__fails_at_second_none_archived, chaser_transaction_fee_setup_fixture)
{
    const auto paid = spend(parent_value - 100);
    const auto unpaid = spend(paid->hash(false), parent_value - 100);
    const auto result = submit(package({ paid, unpaid }), true);
    BC_REQUIRE_EQUAL(result.first, node::error::insufficient_fee);
    BC_REQUIRE_EQUAL(result.second, one);
    BC_REQUIRE(!query_.is_tx(paid->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__test_paid_then_paid_child__success_none_archived, chaser_transaction_fee_setup_fixture)
{
    const auto parent = spend(parent_value - 1'000);
    const auto child = spend(parent->hash(false), parent_value - 2'000);
    const auto result = submit(package({ parent, child }), true);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(!query_.is_tx(parent->hash(false)));
    BC_REQUIRE(!query_.is_tx(child->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__internal_relative_lock__relative_time_locked, chaser_transaction_relative_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend_relative(first->hash(false), parent_value, 1);
    const auto result = submit(package({ first, second }));
    BC_REQUIRE_EQUAL(result.first, system::error::relative_time_locked);
    BC_REQUIRE(!query_.is_tx(first->hash(false)));
}

BC_AUTO_TEST_SUITE_END()
