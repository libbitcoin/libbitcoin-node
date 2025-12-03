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

BOOST_AUTO_TEST_SUITE(parse_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

// General errors

BOOST_AUTO_TEST_CASE(parse__parse_target__empty_path__empty_path)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "?foo=bar"), node::error::empty_path);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__missing_version__missing_version)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/"), node::error::missing_version);
    BOOST_REQUIRE_EQUAL(parse_target(out, "/block/height/123"), node::error::missing_version);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__invalid_version__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/vinvalid/block/height/123"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__version_leading_zero__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v01/block/height/123"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__missing_target__missing_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3"), node::error::missing_target);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__invalid_target__invalid_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/invalid"), node::error::invalid_target);
}

// block/height

BOOST_AUTO_TEST_CASE(parse__parse_target__block_height_valid__expected)
{
    const std::string path = "/v42/block/height/123456";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_height_missing_height__missing_height)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height"), node::error::missing_height);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_height_invalid_height__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/invalid"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_height_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/extra"), node::error::invalid_component);
}

// block/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__block_hash_valid__expected)
{
    const std::string path = "//v42//block//hash//0000000000000000000000000000000000000000000000000000000000000042//?foo=bar";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block");
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
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_hash_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_hash_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_hash_invalid_component__invalid_component)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_component);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_invalid_id_type__invalid_id_type)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/invalid/123"), node::error::invalid_id_type);
}

// header/height

BOOST_AUTO_TEST_CASE(parse__parse_target__header_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "/v42/block/height/123456/header/"));
    BOOST_REQUIRE_EQUAL(request.method, "header");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__header_height_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/header/extra"), node::error::extra_segment);
}

// header/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__header_hash_valid__expected)
{
    const std::string path = "v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/header";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "header");
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
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__header_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/header/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// block_txs/height

BOOST_AUTO_TEST_CASE(parse__parse_target__block_txs_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "/v42/block/height/123456/transactions"));
    BOOST_REQUIRE_EQUAL(request.method, "block_txs");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_txs_height_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/transactions/extra"), node::error::extra_segment);
}

// block_txs/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__block_txs_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/transactions";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_txs");
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
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_txs_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/transactions/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// block_tx/height

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "/v42/block/height/123456/transaction/7"));
    BOOST_REQUIRE_EQUAL(request.method, "block_tx");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);

    const auto position = std::get<uint32_t>(object.at("position").value());
    BOOST_REQUIRE_EQUAL(position, 7u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_height_missing_position__missing_position)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/transaction"), node::error::missing_position);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_height_invalid_position__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/transaction/invalid"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_height_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/transaction/7/extra"), node::error::extra_segment);
}

// block_tx/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/transaction/7";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_tx");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto position = std::get<uint32_t>(object.at("position").value());
    BOOST_REQUIRE_EQUAL(position, 7u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_hash_missing_position__missing_position)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/transaction"), node::error::missing_position);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_hash_invalid_position__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/transaction/invalid"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__block_tx_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/transaction/7/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// transaction

BOOST_AUTO_TEST_CASE(parse__parse_target__transaction_valid__expected)
{
    const std::string path = "/v42/transaction/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
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
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__transaction_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/transaction"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__transaction_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/transaction/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__transaction_invalid_component__invalid_component)
{
    const std::string path = "/v3/transaction/0000000000000000000000000000000000000000000000000000000000000000/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_component);
}

// tx_block

BOOST_AUTO_TEST_CASE(parse__parse_target__tx_block_valid__expected)
{
    const std::string path = "/v42/transaction/0000000000000000000000000000000000000000000000000000000000000042/block";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_block");
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
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__tx_block_invalid_component__invalid_component)
{
    const std::string path = "/v3/transaction/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_component);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__tx_block_extra_segment__extra_segment)
{
    const std::string path = "/v3/transaction/0000000000000000000000000000000000000000000000000000000000000000/block/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// inputs

BOOST_AUTO_TEST_CASE(parse__parse_target__inputs_valid__expected)
{
    const std::string path = "/v255/inputs/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "inputs");
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

BOOST_AUTO_TEST_CASE(parse__parse_target__inputs_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/inputs"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__inputs_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/inputs/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__inputs_extra_segment__extra_segment)
{
    const std::string path = "/v3/inputs/0000000000000000000000000000000000000000000000000000000000000000/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// input

BOOST_AUTO_TEST_CASE(parse__parse_target__input_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "input");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/input"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/input/invalidhex/3"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_missing_component__missing_component)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::missing_component);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_invalid_index__invalid_number)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_number);
}

// input_script

BOOST_AUTO_TEST_CASE(parse__parse_target__input_script_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3/script";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "input_script");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_script_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_script_extra_segment__extra_segment)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/script/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// input_scripts

////BOOST_AUTO_TEST_CASE(parse__parse_target__input_scripts_valid__expected)
////{
////    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/scripts";
////
////    request_t request{};
////    BOOST_REQUIRE(!parse_target(request, path));
////    BOOST_REQUIRE_EQUAL(request.method, "input_scripts");
////    BOOST_REQUIRE(request.params.has_value());
////
////    const auto& params = request.params.value();
////    BOOST_REQUIRE(std::holds_alternative<object_t>(params));
////
////    const auto& object = std::get<object_t>(request.params.value());
////    BOOST_REQUIRE_EQUAL(object.size(), 2u);
////
////    const auto version = std::get<uint8_t>(object.at("version").value());
////    BOOST_REQUIRE_EQUAL(version, 255u);
////
////    const auto& any = std::get<any_t>(object.at("hash").value());
////    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());
////
////    const auto& hash_cptr = any.get<const hash_digest>();
////    BOOST_REQUIRE(hash_cptr);
////    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
////}
////
////BOOST_AUTO_TEST_CASE(parse__parse_target__input_scripts_extra_segment__extra_segment)
////{
////    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/scripts/extra";
////    request_t out{};
////    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
////}

// input_witness

BOOST_AUTO_TEST_CASE(parse__parse_target__input_witness_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3/witness";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "input_witness");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__input_witness_extra_segment__extra_segment)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/witness/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// input_witnesses

////BOOST_AUTO_TEST_CASE(parse__parse_target__input_witnesses_valid__expected)
////{
////    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/witnesses";
////
////    request_t request{};
////    BOOST_REQUIRE(!parse_target(request, path));
////    BOOST_REQUIRE_EQUAL(request.method, "input_witnesses");
////    BOOST_REQUIRE(request.params.has_value());
////
////    const auto& params = request.params.value();
////    BOOST_REQUIRE(std::holds_alternative<object_t>(params));
////
////    const auto& object = std::get<object_t>(request.params.value());
////    BOOST_REQUIRE_EQUAL(object.size(), 2u);
////
////    const auto version = std::get<uint8_t>(object.at("version").value());
////    BOOST_REQUIRE_EQUAL(version, 255u);
////
////    const auto& any = std::get<any_t>(object.at("hash").value());
////    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());
////
////    const auto& hash_cptr = any.get<const hash_digest>();
////    BOOST_REQUIRE(hash_cptr);
////    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
////}
////
////BOOST_AUTO_TEST_CASE(parse__parse_target__input_witnesses_extra_segment__extra_segment)
////{
////    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/witnesses/extra";
////    request_t out{};
////    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
////}

// outputs

BOOST_AUTO_TEST_CASE(parse__parse_target__outputs_valid__expected)
{
    const std::string path = "/v255/outputs/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "outputs");
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

BOOST_AUTO_TEST_CASE(parse__parse_target__outputs_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/outputs"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__outputs_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/outputs/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__outputs_extra_segment__extra_segment)
{
    const std::string path = "/v3/outputs/0000000000000000000000000000000000000000000000000000000000000000/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// output

BOOST_AUTO_TEST_CASE(parse__parse_target__output_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "output");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_missing_component__missing_component)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::missing_component);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_invalid_index__invalid_number)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_number);
}

// output_script

BOOST_AUTO_TEST_CASE(parse__parse_target__output_script_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3/script";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "output_script");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_script_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/invalid";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_script_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/script/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// output_scripts

////BOOST_AUTO_TEST_CASE(parse__parse_target__output_scripts_valid__expected)
////{
////    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/scripts";
////
////    request_t request{};
////    BOOST_REQUIRE(!parse_target(request, path));
////    BOOST_REQUIRE_EQUAL(request.method, "output_scripts");
////    BOOST_REQUIRE(request.params.has_value());
////
////    const auto& params = request.params.value();
////    BOOST_REQUIRE(std::holds_alternative<object_t>(params));
////
////    const auto& object = std::get<object_t>(request.params.value());
////    BOOST_REQUIRE_EQUAL(object.size(), 2u);
////
////    const auto version = std::get<uint8_t>(object.at("version").value());
////    BOOST_REQUIRE_EQUAL(version, 255u);
////
////    const auto& any = std::get<any_t>(object.at("hash").value());
////    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());
////
////    const auto& hash_cptr = any.get<const hash_digest>();
////    BOOST_REQUIRE(hash_cptr);
////    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
////}
////
////BOOST_AUTO_TEST_CASE(parse__parse_target__output_scripts_extra_segment__extra_segment)
////{
////    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/scripts/extra";
////    request_t out{};
////    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
////}

// output_spender

BOOST_AUTO_TEST_CASE(parse__parse_target__output_spender_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3/spender";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "output_spender");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 3u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_spender_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/spender/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// output_spenders

BOOST_AUTO_TEST_CASE(parse__parse_target__output_spenders_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/1/spenders";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "output_spenders");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto index = std::get<uint32_t>(object.at("index").value());
    BOOST_REQUIRE_EQUAL(index, 1u);

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parse__parse_target__output_spenders_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/1/spenders/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// address

BOOST_AUTO_TEST_CASE(parse__parse_target__address_valid__expected)
{
    const std::string path = "/v255/address/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "address");
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

BOOST_AUTO_TEST_CASE(parse__parse_target__address_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/address"), node::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__address_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/address/invalidhex"), node::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__address_extra_segment__extra_segment)
{
    const std::string path = "/v3/address/0000000000000000000000000000000000000000000000000000000000000000/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// filter/height

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "v42/block/height/123456/filter/255"));
    BOOST_REQUIRE_EQUAL(request.method, "filter");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_height_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter/42/extra"), node::error::invalid_subcomponent);
}

// filter/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "filter");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::invalid_subcomponent);
}

// filter_hash/height

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "/v42/block/height/123456/filter/255/hash"));
    BOOST_REQUIRE_EQUAL(request.method, "filter_hash");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_height_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter/42/hash/extra"), node::error::extra_segment);
}

// filter_hash/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255/hash";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "filter_hash");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_hash_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/hash/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

// filter_header/height

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_header_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!parse_target(request, "/v42/block/height/123456/filter/255/header"));
    BOOST_REQUIRE_EQUAL(request.method, "filter_header");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BOOST_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_header_height_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter/42/header/extra"), node::error::extra_segment);
}

// filter_header/hash

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_header_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255/header";

    request_t request{};
    BOOST_REQUIRE(!parse_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "filter_header");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BOOST_REQUIRE_EQUAL(type, 255u);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_header_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/header/extra";
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, path), node::error::extra_segment);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_missing_type_id__missing_type_id)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter"), node::error::missing_type_id);
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter"), node::error::missing_type_id);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_invalid_type__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter/invalid"), node::error::invalid_number);
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/invalid"), node::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parse__parse_target__filter_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/height/123/filter/42/invalid"), node::error::invalid_subcomponent);
    BOOST_REQUIRE_EQUAL(parse_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/invalid"), node::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_SUITE_END()
