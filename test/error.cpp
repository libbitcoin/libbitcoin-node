/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_CASE(error_t__code__success__false_expected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

// database

BOOST_AUTO_TEST_CASE(error_t__code__store_uninitialized__true_expected_message)
{
    constexpr auto value = error::store_uninitialized;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store not initialized");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_reload__true_expected_message)
{
    constexpr auto value = error::store_reload;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store reload");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_prune__true_expected_message)
{
    constexpr auto value = error::store_prune;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store prune");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_snapshot__true_expected_message)
{
    constexpr auto value = error::store_snapshot;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store snapshot");
}

// network

BOOST_AUTO_TEST_CASE(error_t__code__slow_channel__true_expected_message)
{
    constexpr auto value = error::slow_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "slow channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__stalled_channel__true_expected_message)
{
    constexpr auto value = error::stalled_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "stalled channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__exhausted_channel__true_expected_message)
{
    constexpr auto value = error::exhausted_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "exhausted channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__sacrificed_channel__true_expected_message)
{
    constexpr auto value = error::sacrificed_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__suspended_channel__true_expected_message)
{
    constexpr auto value = error::suspended_channel;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed channel");
}

BOOST_AUTO_TEST_CASE(error_t__code__suspended_service__true_expected_message)
{
    constexpr auto value = error::suspended_service;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "sacrificed service");
}

// blockchain

BOOST_AUTO_TEST_CASE(error_t__code__orphan_block__true_expected_message)
{
    constexpr auto value = error::orphan_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "orphan block");
}

BOOST_AUTO_TEST_CASE(error_t__code__orphan_header__true_expected_message)
{
    constexpr auto value = error::orphan_header;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "orphan header");
}

BOOST_AUTO_TEST_CASE(error_t__code__duplicate_block__true_expected_message)
{
    constexpr auto value = error::duplicate_block;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "duplicate block");
}

BOOST_AUTO_TEST_CASE(error_t__code__duplicate_header__true_expected_message)
{
    constexpr auto value = error::duplicate_header;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "duplicate header");
}

// faults

BOOST_AUTO_TEST_CASE(error_t__code__protocol1__true_expected_message)
{
    constexpr auto value = error::protocol1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "protocol1");
}

BOOST_AUTO_TEST_CASE(error_t__code__protocol2__true_expected_message)
{
    constexpr auto value = error::protocol2;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "protocol2");
}

BOOST_AUTO_TEST_CASE(error_t__code__header1__true_expected_message)
{
    constexpr auto value = error::header1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "header1");
}

// TODO: organize2-organize15
BOOST_AUTO_TEST_CASE(error_t__code__organize1__true_expected_message)
{
    constexpr auto value = error::organize1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "organize1");
}

// TODO: validate2-validate6
BOOST_AUTO_TEST_CASE(error_t__code__validate1__true_expected_message)
{
    constexpr auto value = error::validate1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "validate1");
}

// TODO: confirm2-confirm17
BOOST_AUTO_TEST_CASE(error_t__code__confirm1__true_expected_message)
{
    constexpr auto value = error::confirm1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "confirm1");
}

BOOST_AUTO_TEST_SUITE_END()
