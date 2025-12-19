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

BOOST_AUTO_TEST_CASE(error_t__code__success__false_exected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

// database

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

BOOST_AUTO_TEST_CASE(error_t__code__store_prune__true_exected_message)
{
    constexpr auto value = error::store_prune;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "store prune");
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

// faults

BOOST_AUTO_TEST_CASE(error_t__code__protocol1__true_exected_message)
{
    constexpr auto value = error::protocol1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "protocol1");
}

BOOST_AUTO_TEST_CASE(error_t__code__protocol2__true_exected_message)
{
    constexpr auto value = error::protocol2;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "protocol2");
}

BOOST_AUTO_TEST_CASE(error_t__code__header1__true_exected_message)
{
    constexpr auto value = error::header1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "header1");
}

// TODO: organize2-organize15
BOOST_AUTO_TEST_CASE(error_t__code__organize1__true_exected_message)
{
    constexpr auto value = error::organize1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "organize1");
}

// TODO: validate2-validate6
BOOST_AUTO_TEST_CASE(error_t__code__validate1__true_exected_message)
{
    constexpr auto value = error::validate1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "validate1");
}

// TODO: confirm2-confirm17
BOOST_AUTO_TEST_CASE(error_t__code__confirm1__true_exected_message)
{
    constexpr auto value = error::confirm1;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "confirm1");
}

// server (url parse codes)

BOOST_AUTO_TEST_CASE(error_t__code__empty_path__true_exected_message)
{
    constexpr auto value = error::empty_path;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "empty_path");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_number__true_exected_message)
{
    constexpr auto value = error::invalid_number;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_number");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_hash__true_exected_message)
{
    constexpr auto value = error::invalid_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_version__true_exected_message)
{
    constexpr auto value = error::missing_version;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_version");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_target__true_exected_message)
{
    constexpr auto value = error::missing_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_target__true_exected_message)
{
    constexpr auto value = error::invalid_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_hash__true_exected_message)
{
    constexpr auto value = error::missing_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_height__true_exected_message)
{
    constexpr auto value = error::missing_height;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_height");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_position__true_exected_message)
{
    constexpr auto value = error::missing_position;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_position");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_id_type__true_exected_message)
{
    constexpr auto value = error::missing_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_id_type__true_exected_message)
{
    constexpr auto value = error::invalid_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_type_id__true_exected_message)
{
    constexpr auto value = error::missing_type_id;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_type_id");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_component__true_exected_message)
{
    constexpr auto value = error::missing_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_component__true_exected_message)
{
    constexpr auto value = error::invalid_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_subcomponent__true_exected_message)
{
    constexpr auto value = error::invalid_subcomponent;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_subcomponent");
}

BOOST_AUTO_TEST_CASE(error_t__code__extra_segment__true_exected_message)
{
    constexpr auto value = error::extra_segment;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "extra_segment");
}

// server (rpc response codes)

BOOST_AUTO_TEST_CASE(error_t__code__not_found__true_exected_message)
{
    constexpr auto value = error::not_found;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_found");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_argument__true_exected_message)
{
    constexpr auto value = error::invalid_argument;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_argument");
}

BOOST_AUTO_TEST_CASE(error_t__code__not_implemented__true_exected_message)
{
    constexpr auto value = error::not_implemented;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_implemented");
}

BOOST_AUTO_TEST_SUITE_END()
