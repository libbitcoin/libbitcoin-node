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

BC_AUTO_TEST_SUITE(settings_tests)

using namespace bc::network;
using namespace bc::system::chain;

// [node]

BC_AUTO_TEST_CASE(settings__node__default_context__expected)
{
    using namespace network;

    const node::settings node{};
    BC_REQUIRE(node.network == system::chain::selection::none);
    BC_REQUIRE_EQUAL(node.threads, 0_u32);
    BC_REQUIRE_EQUAL(node.delay_inbound, true);
    BC_REQUIRE_EQUAL(node.memory_priority, true);
    BC_REQUIRE_EQUAL(node.thread_priority, true);
    BC_REQUIRE_EQUAL(node.provide_blocks, true);
    BC_REQUIRE_EQUAL(node.require_blocks, true);
    BC_REQUIRE_EQUAL(node.provide_witness, true);
    BC_REQUIRE_EQUAL(node.require_witness, true);
    BC_REQUIRE_EQUAL(node.provide_filters, false);
    BC_REQUIRE_EQUAL(node.provide_privacy, false);
    BC_REQUIRE_EQUAL(node.limited_blocks, false);
    BC_REQUIRE_EQUAL(node.use_gpu, true);
    BC_REQUIRE_EQUAL(node.cpu_batch_verify, false);
    BC_REQUIRE_EQUAL(node.cpu_batch_silent, false);
    BC_REQUIRE_EQUAL(node.minimum_fee_rate, 0.000001);
    BC_REQUIRE_EQUAL(node.minimum_fee_rate_(), 100_u64);
    BC_REQUIRE_EQUAL(node.minimum_bump_rate, 0.0);
    BC_REQUIRE_EQUAL(node.allowed_deviation, 1.5);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
    BC_REQUIRE_EQUAL(node.batch_silent, 1'000'000_u64);
    BC_REQUIRE_EQUAL(node.current_connections, 10_u16);
    BC_REQUIRE_EQUAL(node.announcement_cache, 42_u16);
    BC_REQUIRE_EQUAL(node.fee_estimate_horizon, 0u);
    BC_REQUIRE_EQUAL(node.maximum_height, 0_u32);
    BC_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BC_REQUIRE_EQUAL(node.silent_start_height, 0xffffffff_u32);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 50000_u32);
    BC_REQUIRE_EQUAL(node.maximum_concurrency_(), 50000_size);
    BC_REQUIRE_EQUAL(node.sample_period_seconds, 10_u16);
    BC_REQUIRE_EQUAL(node.compact_timeout_seconds, 10_u16);
    BC_REQUIRE_EQUAL(node.compact_missing_percent, 50_u8);
    BC_REQUIRE_EQUAL(node.currency_window_minutes, 1440_u32);
    ////BC_REQUIRE_EQUAL(node.snapshot_bytes, 200'000'000'000_u64);
    ////BC_REQUIRE_EQUAL(node.snapshot_valid, 250'000_u32);
    ////BC_REQUIRE_EQUAL(node.snapshot_confirm, 500'000_u32);

    BC_REQUIRE_EQUAL(node.threads_(), cores());
    BC_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BC_REQUIRE_EQUAL(node.maximum_concurrency_(), 50'000_size);
    BC_REQUIRE_EQUAL(node.fee_estimate_horizon_(), 0_size);
    BC_REQUIRE(!node.fee_estimate_enabled());
    BC_REQUIRE_EQUAL(node.use_gpu_(), system::batched::accelerated());
    BC_REQUIRE(node.batch_verify_enabled());
    BC_REQUIRE(node.batch_silent_enabled());
    BC_REQUIRE_EQUAL(node.batch_verify_(), node.use_gpu_());
    BC_REQUIRE_EQUAL(node.batch_silent_(), node.use_gpu_());
    BC_REQUIRE(node.sample_period() == steady_clock::duration(seconds(10)));
    BC_REQUIRE(node.currency_window() == steady_clock::duration(minutes(1440)));
    BC_REQUIRE(node.thread_priority_() == network::processing_priority::high);
    BC_REQUIRE(node.memory_priority_() == network::memory_priority::highest);
}

BC_AUTO_TEST_CASE(settings__node__mainnet_context__expected)
{
    const node::settings node{ system::chain::selection::mainnet };
    BC_REQUIRE(node.network == system::chain::selection::mainnet);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 50'000_u32);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
}

BC_AUTO_TEST_CASE(settings__node__testnet3_context__expected)
{
    const node::settings node{ system::chain::selection::testnet3 };
    BC_REQUIRE(node.network == system::chain::selection::testnet3);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 1'000'000_u32);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
}

BC_AUTO_TEST_CASE(settings__node__testnet4_context__expected)
{
    const node::settings node{ system::chain::selection::testnet4 };
    BC_REQUIRE(node.network == system::chain::selection::testnet4);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 100'000_u32);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
}

BC_AUTO_TEST_CASE(settings__node__regtest_context__expected)
{
    const node::settings node{ system::chain::selection::regtest };
    BC_REQUIRE(node.network == system::chain::selection::regtest);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 50'000_u32);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
}

BC_AUTO_TEST_CASE(settings__node__signet_context__expected)
{
    const node::settings node{ system::chain::selection::signet };
    BC_REQUIRE(node.network == system::chain::selection::signet);
    BC_REQUIRE_EQUAL(node.maximum_concurrency, 50'000_u32);
    BC_REQUIRE_EQUAL(node.batch_verify, 1'000'000_u64);
}

BC_AUTO_TEST_CASE(settings__services_provided__default__network_witness)
{
    const node::settings node{};
    BC_REQUIRE_EQUAL(node.services_provided(), 0x0009_u64);
}

BC_AUTO_TEST_CASE(settings__services_provided__filters_privacy__network_witness_filters_v2)
{
    node::settings node{};
    node.provide_filters = true;
    node.provide_privacy = true;
    BC_REQUIRE_EQUAL(node.services_provided(), 0x0849_u64);
}

BC_AUTO_TEST_CASE(settings__use_gpu___disabled__false)
{
    node::settings node{};
    node.use_gpu = false;
    BC_REQUIRE(!node.use_gpu_());
}

BC_AUTO_TEST_CASE(settings__batch_verify___cpu_without_gpu__true)
{
    node::settings node{};
    node.use_gpu = false;
    node.batch_verify = 1;
    node.cpu_batch_verify = true;
    BC_REQUIRE(node.batch_verify_());
}

BC_AUTO_TEST_CASE(settings__batch_verify___no_cpu_without_gpu__false)
{
    node::settings node{};
    node.use_gpu = false;
    node.batch_verify = 1;
    node.cpu_batch_verify = false;
    BC_REQUIRE(!node.batch_verify_());
}

BC_AUTO_TEST_CASE(settings__batch_verify___cpu_unconfigured__false)
{
    node::settings node{};
    node.batch_verify = 0;
    node.cpu_batch_verify = true;
    BC_REQUIRE(!node.batch_verify_());
}

BC_AUTO_TEST_CASE(settings__batch_silent___cpu_without_gpu__true)
{
    node::settings node{};
    node.use_gpu = false;
    node.batch_silent = 1;
    node.cpu_batch_silent = true;
    BC_REQUIRE(node.batch_silent_());
}

BC_AUTO_TEST_CASE(settings__batch_silent___no_cpu_without_gpu__false)
{
    node::settings node{};
    node.use_gpu = false;
    node.batch_silent = 1;
    node.cpu_batch_silent = false;
    BC_REQUIRE(!node.batch_silent_());
}

BC_AUTO_TEST_CASE(settings__batch_silent___cpu_unconfigured__false)
{
    node::settings node{};
    node.batch_silent = 0;
    node.cpu_batch_silent = true;
    BC_REQUIRE(!node.batch_silent_());
}

BC_AUTO_TEST_SUITE_END()
