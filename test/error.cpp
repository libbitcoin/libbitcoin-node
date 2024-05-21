/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(error_tests)

// error_t
// These test std::error_code equality operator overrides.

// general

BOOST_AUTO_TEST_CASE(error_t__code__success__false_exected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

BOOST_AUTO_TEST_CASE(error_t__code__internal_error__true_exected_message)
{
    constexpr auto value = error::internal_error;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "internal error");
}

BOOST_AUTO_TEST_CASE(error_t__code__unexpected_event__true_exected_message)
{
    constexpr auto value = error::unexpected_event;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "unexpected event");
}

// database

BOOST_AUTO_TEST_CASE(error_t__code__store_integrity__true_exected_message)
{
    constexpr auto value = error::store_integrity;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store integrity");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_uninitialized__true_exected_message)
{
    constexpr auto value = error::store_uninitialized;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store not initialized");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_reload__true_exected_message)
{
    constexpr auto value = error::store_reload;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store reload");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_snapshot__true_exected_message)
{
    constexpr auto value = error::store_snapshot;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store snapshot");
}

// network

BOOST_AUTO_TEST_CASE(error_t__code__slow_channel__true_exected_message)
{
    constexpr auto value = error::slow_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "slow channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__stalled_channel__true_exected_message)
{
    constexpr auto value = error::stalled_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "stalled channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__exhausted_channel__true_exected_message)
{
    constexpr auto value = error::exhausted_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "exhausted channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__sacrificed_channel__true_exected_message)
{
    constexpr auto value = error::sacrificed_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__suspended_channel__true_exected_message)
{
    constexpr auto value = error::suspended_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__suspended_service__true_exected_message)
{
    constexpr auto value = error::suspended_service;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed service");
}

// blockchain

BOOST_AUTO_TEST_CASE(error_t__code__orphan_block__true_exected_message)
{
    constexpr auto value = error::orphan_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "orphan block");
}

BOOST_AUTO_TEST_CASE(error_t__code__orphan_header__true_exected_message)
{
    constexpr auto value = error::orphan_header;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "orphan header");
}

BOOST_AUTO_TEST_CASE(error_t__code__duplicate_block__true_exected_message)
{
    constexpr auto value = error::duplicate_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "duplicate block");
}

BOOST_AUTO_TEST_CASE(error_t__code__duplicate_header__true_exected_message)
{
    constexpr auto value = error::duplicate_header;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "duplicate header");
}

BOOST_AUTO_TEST_CASE(error_t__code__malleated_block__true_exected_message)
{
    constexpr auto value = error::malleated_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "malleated block");
}

BOOST_AUTO_TEST_CASE(error_t__code__insufficient_work__true_exected_message)
{
    constexpr auto value = error::insufficient_work;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "insufficient work");
}

BOOST_AUTO_TEST_CASE(error_t__code__validation_bypass__true_exected_message)
{
    constexpr auto value = error::validation_bypass;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "validation bypass");
}

BOOST_AUTO_TEST_CASE(error_t__code__confirmation_bypass__true_exected_message)
{
    constexpr auto value = error::confirmation_bypass;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "confirmation bypass");
}

// query

BOOST_AUTO_TEST_CASE(error_t__code__set_block_unconfirmable__true_exected_message)
{
    constexpr auto value = error::set_block_unconfirmable;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_block_unconfirmable");
}

////BOOST_AUTO_TEST_CASE(error_t__code__set_block_link__true_exected_message)
////{
////    constexpr auto value = error::set_block_link;
////    const auto ec = code(value);
////    BOOST_REQUIRE(ec);
////    BOOST_REQUIRE(ec == value);
////    BOOST_REQUIRE_EQUAL(ec.message(), "set_block_link");
////}

BOOST_AUTO_TEST_CASE(error_t__code__get_height__true_exected_message)
{
    constexpr auto value = error::get_height;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_height");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_branch_work__true_exected_message)
{
    constexpr auto value = error::get_branch_work;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_branch_work");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_is_strong__true_exected_message)
{
    constexpr auto value = error::get_is_strong;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_is_strong");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_branch_point__true_exected_message)
{
    constexpr auto value = error::invalid_branch_point;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_branch_point");
}

BOOST_AUTO_TEST_CASE(error_t__code__pop_candidate__true_exected_message)
{
    constexpr auto value = error::pop_candidate;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "pop_candidate");
}

BOOST_AUTO_TEST_CASE(error_t__code__push_candidate__true_exected_message)
{
    constexpr auto value = error::push_candidate;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "push_candidate");
}

BOOST_AUTO_TEST_CASE(error_t__code__set_header_link__true_exected_message)
{
    constexpr auto value = error::set_header_link;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_header_link");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_fork_point__true_exected_message)
{
    constexpr auto value = error::invalid_fork_point;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_fork_point");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_candidate_chain_state__true_exected_message)
{
    constexpr auto value = error::get_candidate_chain_state;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_candidate_chain_state");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_block__true_exected_message)
{
    constexpr auto value = error::get_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_block");
}

BOOST_AUTO_TEST_CASE(error_t__code__set_dissasociated__true_exected_message)
{
    constexpr auto value = error::set_dissasociated;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_dissasociated");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_unassociated__true_exected_message)
{
    constexpr auto value = error::get_unassociated;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_unassociated");
}

BOOST_AUTO_TEST_CASE(error_t__code__get_fork_work__true_exected_message)
{
    constexpr auto value = error::get_fork_work;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "get_fork_work");
}

BOOST_AUTO_TEST_CASE(error_t__code__to_confirmed__true_exected_message)
{
    constexpr auto value = error::to_confirmed;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "to_confirmed");
}

BOOST_AUTO_TEST_CASE(error_t__code__pop_confirmed__true_exected_message)
{
    constexpr auto value = error::pop_confirmed;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "pop_confirmed");
}

BOOST_AUTO_TEST_CASE(error_t__code__set_confirmed__true_exected_message)
{
    constexpr auto value = error::set_confirmed;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_confirmed");
}

BOOST_AUTO_TEST_CASE(error_t__code__block_confirmable__true_exected_message)
{
    constexpr auto value = error::block_confirmable;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "block_confirmable");
}

BOOST_AUTO_TEST_CASE(error_t__code__set_txs_connected__true_exected_message)
{
    constexpr auto value = error::set_txs_connected;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_txs_connected");
}

BOOST_AUTO_TEST_CASE(error_t__code__set_block_valid__true_exected_message)
{
    constexpr auto value = error::set_block_valid;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "set_block_valid");
}

// query composite

BOOST_AUTO_TEST_CASE(error_t__code__node_push__true_exected_message)
{
    constexpr auto value = error::node_push;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "node_push");
}

BOOST_AUTO_TEST_CASE(error_t__code__node_confirm__true_exected_message)
{
    constexpr auto value = error::node_confirm;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "node_confirm");
}

BOOST_AUTO_TEST_CASE(error_t__code__node_validate__true_exected_message)
{
    constexpr auto value = error::node_validate;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "node_validate");
}

BOOST_AUTO_TEST_CASE(error_t__code__node_roll_back__true_exected_message)
{
    constexpr auto value = error::node_roll_back;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "node_roll_back");
}

BOOST_AUTO_TEST_SUITE_END()
