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

BOOST_AUTO_TEST_CASE(path_to_request__transaction_valid_path__expected_request)
{
    const std::string valid_path = "/v42/transaction/0000000000000000000000000000000000000000000000000000000000000042";

    const auto request = path_to_request(valid_path);
    BOOST_REQUIRE_EQUAL(request.method, "transaction");

    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(encode_hash(*hash_cptr), "0000000000000000000000000000000000000000000000000000000000000042");
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_missing_hash__throws_exception)
{
    const std::string invalid_path = "/v3/transaction";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "missing transaction hash");
    }
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_invalid_hash__throws_exception)
{
    const std::string invalid_path = "/v3/transaction/invalidhex";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "invalid hash");
    }
}

BOOST_AUTO_TEST_CASE(path_to_request__transaction_extra_segments__throws_exception)
{
    const std::string invalid_path = "/v3/transaction/0000000000000000000000000000000000000000000000000000000000000000/extra";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "extra segments");
    }
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_valid_path__expected_request)
{
    const std::string valid_path = "/v42/block/height/123456";

    const auto request = path_to_request(valid_path);
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

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    // any/ptr is not currently nullable, so use hash sentinel.
    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(*hash_cptr, null_hash);
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_missing_height__throws_exception)
{
    const std::string invalid_path = "/v3/block/height";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "missing block height");
    }
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_invalid_height__throws_exception)
{
    const std::string invalid_path = "/v3/block/height/invalid";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "invalid number");
    }
}

BOOST_AUTO_TEST_CASE(path_to_request__block_height_extra_segments__throws_exception)
{
    const std::string invalid_path = "/v3/block/height/123/extra";
    BOOST_REQUIRE_THROW(path_to_request(invalid_path), std::runtime_error);
    try
    {
        path_to_request(invalid_path);
    }
    catch (const std::runtime_error& ex)
    {
        BOOST_REQUIRE_EQUAL(ex.what(), "invalid block component");
    }
}

BOOST_AUTO_TEST_SUITE_END()