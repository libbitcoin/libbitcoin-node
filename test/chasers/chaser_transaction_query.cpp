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

BC_AUTO_TEST_SUITE(chaser_transaction_query_tests)

// build_template

BC_FIXTURE_TEST_CASE(chaser_transaction__build_template__relay_disabled__pooling_disabled, chaser_transaction_setup_fixture)
{
    const auto [ec, height, links] = build_template();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(links.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__build_template__empty_pool__empty_at_next_height, chaser_transaction_pooling_setup_fixture)
{
    const auto [ec, height, links] = build_template();
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(height, 2u);
    BC_REQUIRE(links.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__build_template__parent_and_child__parent_first, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto result = submit(package({ first, second }));
    const auto [ec, height, links] = build_template();
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(links.size(), 2u);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(first->hash(false)));
    BC_REQUIRE_EQUAL(links.back(), query_.to_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__build_template__conflict__higher_fee_only, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_result = submit(package({ first }));
    const auto second_result = submit(package({ second }));
    const auto [ec, height, links] = build_template();
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(links.size(), 1u);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__build_template__child_of_losing_rival__excluded, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto child = spend(first->hash(false), parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_result = submit(package({ first, child }));
    const auto second_result = submit(package({ second }));
    const auto [ec, height, links] = build_template();
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(links.size(), 1u);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(second->hash(false)));
}

// get_confirmable_txs

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_txs__relay_disabled__pooling_disabled, chaser_transaction_setup_fixture)
{
    const auto [ec, links] = get_confirmable_txs();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(links.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_txs__not_current__pooling_disabled, chaser_transaction_relay_setup_fixture)
{
    const auto [ec, links] = get_confirmable_txs();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(links.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_txs__empty_pool__empty, chaser_transaction_pooling_setup_fixture)
{
    const auto [ec, links] = get_confirmable_txs();
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE(links.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_txs__parent_and_child__pool_order, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto result = submit(package({ first, second }));
    const auto [ec, links] = get_confirmable_txs();
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(links.size(), 2u);
    BC_REQUIRE_EQUAL(links.front(), query_.to_tx(first->hash(false)));
    BC_REQUIRE_EQUAL(links.back(), query_.to_tx(second->hash(false)));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_txs__relay_off_pooled_row__pooling_disabled, chaser_transaction_relay_off_setup_fixture)
{
    const auto [ec, links] = get_confirmable_txs();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(links.empty());
}

// get_confirmable_fees

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_fees__relay_disabled__pooling_disabled, chaser_transaction_setup_fixture)
{
    const auto [ec, rates] = get_confirmable_fees();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(rates.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_fees__not_current__pooling_disabled, chaser_transaction_relay_setup_fixture)
{
    const auto [ec, rates] = get_confirmable_fees();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(rates.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_fees__empty_pool__empty, chaser_transaction_pooling_setup_fixture)
{
    const auto [ec, rates] = get_confirmable_fees();
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE(rates.empty());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_fees__pooled_tx__fee_and_size, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto result = submit(package({ tx }));
    const auto [ec, rates] = get_confirmable_fees();
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, database::error::success);
    BC_REQUIRE_EQUAL(rates.size(), 1u);
    BC_REQUIRE_EQUAL(rates.front().fee, 1'000u);
    BC_REQUIRE_EQUAL(rates.front().bytes, tx->virtual_size());
    BC_REQUIRE_EQUAL(rates.front().pooled_height, 2u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__get_confirmable_fees__relay_off_pooled_row__pooling_disabled, chaser_transaction_relay_off_setup_fixture)
{
    const auto [ec, rates] = get_confirmable_fees();
    BC_REQUIRE_EQUAL(ec, node::error::pooling_disabled);
    BC_REQUIRE(rates.empty());
}

// estimate

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__relay_disabled__estimate_disabled, chaser_transaction_setup_fixture)
{
    BC_REQUIRE_EQUAL(estimate(0).first, node::error::estimate_disabled);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__not_current__estimate_premature, chaser_transaction_relay_setup_fixture)
{
    BC_REQUIRE_EQUAL(estimate(0).first, node::error::estimate_premature);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__unknown_mode__estimate_false, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE_EQUAL(estimate(0, node::estimate_mode::unknown).first, node::error::estimate_false);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__target_at_horizon__estimate_false, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE_EQUAL(estimate(node::maximum_estimate_horizon).first, node::error::estimate_false);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__no_block_space__estimate_false, chaser_transaction_reserve_setup_fixture)
{
    BC_REQUIRE_EQUAL(estimate(0).first, node::error::estimate_false);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__empty_pool__zero, chaser_transaction_pooling_setup_fixture)
{
    const auto [ec, rate] = estimate(0);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__empty_pool_minimum_fee_rate__minimum_fee_rate, chaser_transaction_fee_setup_fixture)
{
    const auto [ec, rate] = estimate(0);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 1'526u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__full_block__above_lowest_take, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto [ec, rate] = estimate(0);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__full_block_without_room__estimate_false, chaser_transaction_room_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(estimate(0).first, node::error::estimate_false);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__open_block__zero, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto [ec, rate] = estimate(1);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__after_template__continued, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto [built_ec, height, links] = build_template();
    const auto next = estimate(0);
    const auto after = estimate(1);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(built_ec, database::error::success);
    BC_REQUIRE_EQUAL(links.size(), 1u);
    BC_REQUIRE_EQUAL(next.first, node::error::success);
    BC_REQUIRE_EQUAL(next.second, 16'394u);
    BC_REQUIRE_EQUAL(after.first, node::error::success);
    BC_REQUIRE_EQUAL(after.second, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__pool_written_projection_held__unchanged, chaser_transaction_held_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto first_result = submit(package({ first }));
    const auto before = estimate(0);
    const auto second_result = submit(package({ second }));
    const auto after = estimate(0);
    const auto deeper = estimate(1);
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(before.first, node::error::success);
    BC_REQUIRE_EQUAL(before.second, 0u);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE_EQUAL(after.first, node::error::success);
    BC_REQUIRE_EQUAL(after.second, 0u);
    BC_REQUIRE_EQUAL(deeper.first, node::error::success);
    BC_REQUIRE_EQUAL(deeper.second, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__block_organized_projection_held__projected_again, chaser_transaction_held_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto first_result = submit(package({ first }));
    const auto before = estimate(0);
    const auto second_result = submit(package({ second }));
    const auto confirmed = !confirm().is_terminal();
    const auto current = bumped(3);
    const auto after = estimate(0);
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(before.first, node::error::success);
    BC_REQUIRE_EQUAL(before.second, 0u);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE(confirmed);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(after.first, node::error::success);
    BC_REQUIRE_EQUAL(after.second, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__pool_written__projected_again, chaser_transaction_live_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto first_result = submit(package({ first }));
    const auto before = estimate(0);
    const auto second_result = submit(package({ second }));
    const auto after = estimate(0);
    BC_REQUIRE_EQUAL(first_result.first, node::error::success);
    BC_REQUIRE_EQUAL(before.first, node::error::success);
    BC_REQUIRE_EQUAL(before.second, 0u);
    BC_REQUIRE_EQUAL(second_result.first, node::error::success);
    BC_REQUIRE_EQUAL(after.first, node::error::success);
    BC_REQUIRE_EQUAL(after.second, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__basic_after_block__no_arrivals, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto confirmed = !confirm().is_terminal();
    const auto current = bumped(3);
    const auto [ec, rate] = estimate(1, node::estimate_mode::basic);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(confirmed);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__economical_after_block__arrivals_fill_open_block, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto confirmed = !confirm().is_terminal();
    const auto current = bumped(3);
    const auto [ec, rate] = estimate(1, node::estimate_mode::economical);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(confirmed);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__conservative_after_block__arrivals_fill_open_block, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto confirmed = !confirm().is_terminal();
    const auto current = bumped(3);
    const auto [ec, rate] = estimate(1, node::estimate_mode::conservative);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(confirmed);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__estimate__geometric_after_block__arrivals_fill_open_block, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto confirmed = !confirm().is_terminal();
    const auto current = bumped(3);
    const auto [ec, rate] = estimate(1, node::estimate_mode::geometric);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(confirmed);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(rate, 16'394u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__pruned__arrivals_below_floor__not_counted, chaser_transaction_single_setup_fixture)
{
    const auto first = spend(parent_value - 1'000);
    const auto second = spend(first->hash(false), parent_value - 3'000);
    const auto result = submit(package({ first, second }));
    const auto link = confirm();
    const auto current = bumped(3);
    const auto counted = estimate(1, node::estimate_mode::economical);
    node_.notify(node::error::success, chases::pruned{ link });
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(current);
    BC_REQUIRE_EQUAL(counted.second, 16'394u);
    BC_REQUIRE(estimated_at(1, node::estimate_mode::economical, 0));
}

BC_AUTO_TEST_SUITE_END()
