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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(rest_parser_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

// transaction

BOOST_AUTO_TEST_CASE(path_to_request__transaction_valid__expected)
{
    const std::string path = "/v255/transaction/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!path_to_request(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "transaction");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, "/v3/transaction"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, "/v3/transaction/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_extra_segment__extra_segment)
{
    const std::string path = "/v3/transaction/0000000000000000000000000000000000000000000000000000000000000000/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, path), node::error::extra_segment);
}

// block/height

BOOST_AUTO_TEST_CASE(path_to_request__block_height_valid___expected_request)
{
    request_t request{};
    BOOST_REQUIRE(!path_to_request(request, "/v42/block/height/123456"));
    BOOST_REQUIRE_EQUAL(request.method, "block");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_missing_height__missing_height)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, "/v3/block/height"), node::error::missing_height);
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_invalid_height__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, "/v3/block/height/invalid"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(path_to_request(out, "/v3/block/height/123/extra"), node::error::invalid_component);
}

BOOST_AUTO_TEST_SUITE_END()