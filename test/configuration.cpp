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
#include "test.hpp"

using namespace bc::system;
using namespace bc::network;

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const node::configuration instance(chain::selection::none);

    // Just a sample of settings.
    BOOST_REQUIRE_EQUAL(instance.network.threads, 0u);
    BOOST_REQUIRE_EQUAL(instance.bitcoin.first_version, 1u);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__mainnet__success_peer_identifier_mainnet_magic)
{
    node::configuration instance(chain::selection::mainnet);
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.network.peer.identifier, 0xd9b4bef9_u32);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__signet__success_peer_identifier_signet_magic)
{
    node::configuration instance(chain::selection::signet);
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.network.peer.identifier, 0x40cf030a_u32);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__custom_signet__success_peer_identifier_challenge_magic)
{
    node::configuration instance(chain::selection::signet);
    instance.bitcoin.signet_challenge = system::config::script{ "1" };
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.network.peer.identifier, from_little_endian<uint32_t>(bitcoin_hash(base16_chunk("0151"))));
}

BOOST_AUTO_TEST_CASE(configuration__initialize__silent_without_witness__silent_unwitnessed)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.silent_start_height = 0;
    instance.node.require_witness = false;
    BOOST_REQUIRE_EQUAL(instance.initialize(), node::error::silent_unwitnessed);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__silent_with_witness__success)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.silent_start_height = 0;
    instance.node.require_witness = true;
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_CASE(configuration__initialize__limited_silent_at_bypassed__silent_pruned)
{
    node::configuration instance(chain::selection::mainnet);
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = true;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(bypassed);
    BOOST_REQUIRE_EQUAL(instance.initialize(), node::error::silent_pruned);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__limited_silent_above_bypassed__success)
{
    node::configuration instance(chain::selection::mainnet);
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = true;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(add1(bypassed));
    instance.database.filter_bk.buckets = 0;
    instance.database.filter_tx.buckets = 0;
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_CASE(configuration__initialize__unlimited_silent_at_bypassed__success)
{
    node::configuration instance(chain::selection::mainnet);
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = false;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(bypassed);
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_CASE(configuration__initialize__limited_filter__filter_pruned)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.limited_blocks = true;
    instance.database.filter_bk.buckets = 1;
    instance.database.filter_tx.buckets = 1;
    BOOST_REQUIRE_EQUAL(instance.initialize(), node::error::filter_pruned);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__limited_partial_filter__success)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.limited_blocks = true;
    instance.database.filter_bk.buckets = 1;
    instance.database.filter_tx.buckets = 0;
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_CASE(configuration__initialize__unlimited_filter__success)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.limited_blocks = false;
    instance.database.filter_bk.buckets = 1;
    instance.database.filter_tx.buckets = 1;
    BOOST_REQUIRE(!instance.initialize());
}

BOOST_AUTO_TEST_CASE(configuration__initialize__cpu_batching_without_gpu__tables_sized)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.use_gpu = false;
    instance.node.batch_verify = 1;
    instance.node.batch_silent = 1;
    instance.node.cpu_batch_verify = true;
    instance.node.cpu_batch_silent = true;
    instance.database.ecdsa.size = 42;
    instance.database.schnorr.size = 42;
    instance.database.silent.size = 42;
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.database.ecdsa.size, 42u);
    BOOST_REQUIRE_EQUAL(instance.database.schnorr.size, 42u);
    BOOST_REQUIRE_EQUAL(instance.database.silent.size, 42u);
    BOOST_REQUIRE_EQUAL(instance.database.gpu_scan_minimum, max_uint32);
}

BOOST_AUTO_TEST_CASE(configuration__initialize__no_batching_without_gpu__tables_zero)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.use_gpu = false;
    instance.node.batch_verify = 1;
    instance.node.batch_silent = 1;
    instance.node.cpu_batch_verify = false;
    instance.node.cpu_batch_silent = false;
    instance.database.ecdsa.size = 42;
    instance.database.schnorr.size = 42;
    instance.database.silent.size = 42;
    BOOST_REQUIRE(!instance.initialize());
    BOOST_REQUIRE_EQUAL(instance.database.ecdsa.size, 0u);
    BOOST_REQUIRE_EQUAL(instance.database.schnorr.size, 0u);
    BOOST_REQUIRE_EQUAL(instance.database.silent.size, 0u);
    BOOST_REQUIRE_EQUAL(instance.database.gpu_scan_minimum, max_uint32);
}

BOOST_AUTO_TEST_SUITE_END()
