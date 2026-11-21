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

BC_AUTO_TEST_SUITE(configuration_tests)

BC_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const node::configuration instance(chain::selection::none);

    // Just a sample of settings.
    BC_REQUIRE_EQUAL(instance.network.threads, 0u);
    BC_REQUIRE_EQUAL(instance.bitcoin.first_version, 1u);
}

BC_AUTO_TEST_CASE(configuration__relay_slots__unset__manual_peers_and_current_connections)
{
    node::configuration instance(chain::selection::mainnet);
    instance.network.manual.peers.emplace_back("127.0.0.1:8333");
    instance.network.manual.peers.emplace_back("127.0.0.2:8333");
    BC_REQUIRE_EQUAL(instance.manual_relay_slots(), 2u);
    BC_REQUIRE_EQUAL(instance.outbound_relay_slots(), 10u);
}

BC_AUTO_TEST_CASE(configuration__relay_slots__unset_no_current_connections__outbound_connections_limited)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.current_connections = 0;
    BC_REQUIRE_EQUAL(instance.manual_relay_slots(), 0u);
    BC_REQUIRE_EQUAL(instance.outbound_relay_slots(), 64u);
}

BC_AUTO_TEST_CASE(configuration__relay_slots__configured__configured)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.manual_relay_slots = 4;
    instance.node.outbound_relay_slots = 20;
    BC_REQUIRE_EQUAL(instance.manual_relay_slots(), 4u);
    BC_REQUIRE_EQUAL(instance.outbound_relay_slots(), 20u);
}

BC_AUTO_TEST_CASE(configuration__relay_slots__excessive__limited)
{
    node::configuration instance(chain::selection::mainnet);
    instance.node.manual_relay_slots = 60;
    instance.node.outbound_relay_slots = 20;
    BC_REQUIRE_EQUAL(instance.manual_relay_slots(), 60u);
    BC_REQUIRE_EQUAL(instance.outbound_relay_slots(), 4u);
}

BC_AUTO_TEST_CASE(configuration__initialize__unset_relay_slots__derived)
{
    node::configuration instance(chain::selection::mainnet);
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.node.manual_relay_slots, 0_u16);
    BC_REQUIRE_EQUAL(instance.node.outbound_relay_slots, 10_u16);
}

BC_AUTO_TEST_CASE(configuration__initialize__mainnet__success_peer_identifier_mainnet_magic)
{
    node::configuration instance(chain::selection::mainnet);
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.network.peer.identifier, 0xd9b4bef9_u32);
}

BC_AUTO_TEST_CASE(configuration__initialize__signet__success_peer_identifier_signet_magic)
{
    node::configuration instance(chain::selection::signet);
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.network.peer.identifier, 0x40cf030a_u32);
}

BC_AUTO_TEST_CASE(configuration__initialize__custom_signet__success_peer_identifier_challenge_magic)
{
    node::configuration instance(chain::selection::signet);
    instance.bitcoin.signet_challenge = system::config::script{ "1" };
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.network.peer.identifier, from_little_endian<uint32_t>(bitcoin_hash(base16_chunk("0151"))));
}

BC_AUTO_TEST_CASE(configuration__initialize__silent_without_witness__silent_unwitnessed)
{
    node::configuration instance(test::node_configuration());
    instance.node.silent_start_height = 0;
    instance.node.require_witness = false;
    BC_REQUIRE_EQUAL(instance.initialize(), node::error::silent_unwitnessed);
}

BC_AUTO_TEST_CASE(configuration__initialize__silent_with_witness__success)
{
    node::configuration instance(test::node_configuration());
    instance.node.silent_start_height = 0;
    instance.node.require_witness = true;
    BC_REQUIRE(!instance.initialize());
}

BC_AUTO_TEST_CASE(configuration__initialize__limited_silent_at_bypassed__silent_pruned)
{
    node::configuration instance(test::node_configuration());
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = true;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(bypassed);
    BC_REQUIRE_EQUAL(instance.initialize(), node::error::silent_pruned);
}

BC_AUTO_TEST_CASE(configuration__initialize__limited_silent_above_bypassed__success)
{
    node::configuration instance(test::node_configuration());
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = true;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(add1(bypassed));
    instance.database.filter_bk.buckets = 0;
    instance.database.filter_tx.buckets = 0;
    BC_REQUIRE(!instance.initialize());
}

BC_AUTO_TEST_CASE(configuration__initialize__unlimited_silent_at_bypassed__success)
{
    node::configuration instance(test::node_configuration());
    const auto bypassed = std::max(instance.bitcoin.top_checkpoint().height(), instance.bitcoin.milestone.height());
    instance.node.limited_blocks = false;
    instance.node.silent_start_height = possible_narrow_cast<uint32_t>(bypassed);
    BC_REQUIRE(!instance.initialize());
}

BC_AUTO_TEST_CASE(configuration__initialize__limited_filter__filter_pruned)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = true;
    instance.node.provide_filters = true;
    BC_REQUIRE_EQUAL(instance.initialize(), node::error::filter_pruned);
}

BC_AUTO_TEST_CASE(configuration__initialize__cutoff_above_minimum__excessive_cutoff)
{
    node::configuration instance(test::node_configuration());
    instance.node.pool.minimum_fee_rate = 0.00001;
    instance.node.pool.cutoff_rate = 0.00002;
    BC_REQUIRE_EQUAL(instance.initialize(), node::error::excessive_cutoff);
}

BC_AUTO_TEST_CASE(configuration__initialize__cutoff_at_minimum__success)
{
    node::configuration instance(test::node_configuration());
    instance.node.pool.minimum_fee_rate = 0.00001;
    instance.node.pool.cutoff_rate = 0.00001;
    BC_REQUIRE(!instance.initialize());
}

BC_AUTO_TEST_CASE(configuration__initialize__limited_unfiltered__success)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = true;
    instance.node.provide_filters = false;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE(!instance.database.envelope.provide_filters);
}

BC_AUTO_TEST_CASE(configuration__initialize__unlimited_filter__enveloped)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = false;
    instance.node.provide_filters = true;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE(instance.database.envelope.provide_filters);
}

BC_AUTO_TEST_CASE(configuration__initialize__unbucketed_filter__enveloped)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = false;
    instance.node.provide_filters = true;
    instance.database.filter_bk.buckets = 0;
    instance.database.filter_tx.buckets = 0;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE(instance.database.envelope.provide_filters);
}

BC_AUTO_TEST_CASE(configuration__initialize__unfiltered__filter_buckets_zero)
{
    node::configuration instance(test::node_configuration());
    instance.node.provide_filters = false;
    instance.database.filter_bk.buckets = 42;
    instance.database.filter_tx.buckets = 42;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.database.filter_bk.buckets, 0u);
    BC_REQUIRE_EQUAL(instance.database.filter_tx.buckets, 0u);
}

BC_AUTO_TEST_CASE(configuration__initialize__filtered__filter_buckets_unchanged)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = false;
    instance.node.provide_filters = true;
    instance.database.filter_bk.buckets = 42;
    instance.database.filter_tx.buckets = 42;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.database.filter_bk.buckets, 42u);
    BC_REQUIRE_EQUAL(instance.database.filter_tx.buckets, 42u);
}

BC_AUTO_TEST_CASE(configuration__initialize__unsilent__scan_buckets_zero)
{
    node::configuration instance(test::node_configuration());
    instance.node.silent_start_height = max_uint32;
    instance.database.scan.buckets = 42;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.database.scan.buckets, 0u);
}

BC_AUTO_TEST_CASE(configuration__initialize__silent__scan_buckets_unchanged)
{
    node::configuration instance(test::node_configuration());
    instance.node.limited_blocks = false;
    instance.node.require_witness = true;
    instance.node.silent_start_height = 0;
    instance.database.scan.buckets = 42;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.database.scan.buckets, 42u);
}

BC_AUTO_TEST_CASE(configuration__initialize__witness__enveloped)
{
    node::configuration instance(test::node_configuration());
    instance.node.require_witness = true;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE(instance.database.envelope.require_witness);
}

BC_AUTO_TEST_CASE(configuration__initialize__cpu_batching_without_gpu__tables_sized)
{
    node::configuration instance(test::node_configuration());
    instance.node.use_gpu = false;
    instance.node.batch_verify = 1;
    instance.node.batch_silent = 1;
    instance.node.cpu_batch_verify = true;
    instance.node.cpu_batch_silent = true;
    instance.database.ecdsa.size = 42;
    instance.database.schnorr.size = 42;
    instance.database.silent.size = 42;
    instance.database.prevalid.size = 42;
    instance.database.presilent.size = 42;
    const auto ec = instance.initialize();
    system::batched::accelerate(true);
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(instance.database.ecdsa.size, 42u);
    BC_REQUIRE_EQUAL(instance.database.schnorr.size, 42u);
    BC_REQUIRE_EQUAL(instance.database.silent.size, 42u);
    BC_REQUIRE_EQUAL(instance.database.prevalid.size, 42u);
    BC_REQUIRE_EQUAL(instance.database.presilent.size, 42u);
    BC_REQUIRE_EQUAL(instance.database.gpu_scan_minimum, max_uint32);
}

BC_AUTO_TEST_CASE(configuration__initialize__no_batching_without_gpu__tables_zero)
{
    node::configuration instance(test::node_configuration());
    instance.node.use_gpu = false;
    instance.node.batch_verify = 1;
    instance.node.batch_silent = 1;
    instance.node.cpu_batch_verify = false;
    instance.node.cpu_batch_silent = false;
    instance.database.ecdsa.size = 42;
    instance.database.schnorr.size = 42;
    instance.database.silent.size = 42;
    instance.database.prevalid.size = 42;
    instance.database.presilent.size = 42;
    const auto ec = instance.initialize();
    system::batched::accelerate(true);
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(instance.database.ecdsa.size, 0u);
    BC_REQUIRE_EQUAL(instance.database.schnorr.size, 0u);
    BC_REQUIRE_EQUAL(instance.database.silent.size, 0u);
    BC_REQUIRE_EQUAL(instance.database.prevalid.size, 0u);
    BC_REQUIRE_EQUAL(instance.database.presilent.size, 0u);
    BC_REQUIRE_EQUAL(instance.database.gpu_scan_minimum, max_uint32);
}

BC_AUTO_TEST_CASE(configuration__initialize__without_gpu__not_accelerated)
{
    node::configuration instance(test::node_configuration());
    instance.node.use_gpu = false;
    const auto ec = instance.initialize();
    const auto accelerated = system::batched::accelerated();
    system::batched::accelerate(true);
    BC_REQUIRE(!ec);
    BC_REQUIRE(!accelerated);
}

BC_AUTO_TEST_SUITE_END()
