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

BOOST_AUTO_TEST_CASE(error_t__code__success__false_exected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

BOOST_AUTO_TEST_CASE(error_t__code__unknown__true_exected_message)
{
    constexpr auto value = error::unknown;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "unknown error");
}

BOOST_AUTO_TEST_CASE(error_t__code__unexpected_event__true_exected_message)
{
    constexpr auto value = error::unexpected_event;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "unexpected event");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_uninitialized__true_exected_message)
{
    constexpr auto value = error::store_uninitialized;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store not initialized");
}

BOOST_AUTO_TEST_CASE(error_t__code__store_integrity__true_exected_message)
{
    constexpr auto value = error::store_integrity;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store integrity");
}

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

BOOST_AUTO_TEST_CASE(error_t__code__insufficient_work__true_exected_message)
{
    constexpr auto value = error::insufficient_work;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "insufficient work");
}

BOOST_AUTO_TEST_SUITE_END()
