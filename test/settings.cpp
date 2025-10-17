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

BOOST_AUTO_TEST_SUITE(settings_tests)

using namespace bc::network;

// [log]

BOOST_AUTO_TEST_CASE(settings__log__default_context__expected)
{
    const log::settings log{};
    BOOST_REQUIRE_EQUAL(log.application, levels::application_defined);
    BOOST_REQUIRE_EQUAL(log.news, levels::news_defined);
    BOOST_REQUIRE_EQUAL(log.session, levels::session_defined);
    BOOST_REQUIRE_EQUAL(log.protocol, false /*levels::protocol_defined*/);
    BOOST_REQUIRE_EQUAL(log.proxy, false /*levels::proxy_defined*/);
    BOOST_REQUIRE_EQUAL(log.remote, levels::remote_defined);
    BOOST_REQUIRE_EQUAL(log.fault, levels::fault_defined);
    BOOST_REQUIRE_EQUAL(log.quitting, false /*levels::quitting_defined*/);
    BOOST_REQUIRE_EQUAL(log.objects, false /*levels::objects_defined*/);
    BOOST_REQUIRE_EQUAL(log.verbose, false /*levels::verbose_defined*/);
    BOOST_REQUIRE_EQUAL(log.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(log.path, "");
    BOOST_REQUIRE_EQUAL(log.log_file1(), "bn_end.log");
    BOOST_REQUIRE_EQUAL(log.log_file2(), "bn_begin.log");
    BOOST_REQUIRE_EQUAL(log.events_file(), "events.log");
#if defined(HAVE_MSC)
    BOOST_REQUIRE_EQUAL(log.symbols, "");
#endif
}

// [node]

BOOST_AUTO_TEST_CASE(settings__node__default_context__expected)
{
    using namespace network;

    const node::settings node{};
    BOOST_REQUIRE_EQUAL(node.priority, true);
    BOOST_REQUIRE_EQUAL(node.delay_inbound, true);
    BOOST_REQUIRE_EQUAL(node.headers_first, true);
    BOOST_REQUIRE_EQUAL(node.allowed_deviation, 1.5);
    BOOST_REQUIRE_EQUAL(node.announcement_cache, 42_u16);
    BOOST_REQUIRE_EQUAL(node.allocation_multiple, 20_u16);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_bytes, 200'000'000'000_u64);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_valid, 250'000_u32);
    ////BOOST_REQUIRE_EQUAL(node.snapshot_confirm, 500'000_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_height, 0_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency, 50000_u32);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency_(), 50000_size);
    BOOST_REQUIRE_EQUAL(node.sample_period_seconds, 10_u16);
    BOOST_REQUIRE_EQUAL(node.currency_window_minutes, 60_u32);
    BOOST_REQUIRE_EQUAL(node.threads, 1_u32);

    BOOST_REQUIRE_EQUAL(node.threads_(), one);
    BOOST_REQUIRE_EQUAL(node.maximum_height_(), max_size_t);
    BOOST_REQUIRE_EQUAL(node.maximum_concurrency_(), 50'000_size);
    BOOST_REQUIRE(node.sample_period() == steady_clock::duration(seconds(10)));
    BOOST_REQUIRE(node.currency_window() == steady_clock::duration(minutes(60)));
    BOOST_REQUIRE(node.priority_() == network::thread_priority::high);
}

// [server]
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(server__html_server__defaults__expected)
{
    const server::settings::html_server instance{};

    // tcp_server
    BOOST_REQUIRE(instance.name.empty());
    BOOST_REQUIRE(!instance.secure);
    BOOST_REQUIRE(instance.binds.empty());
    BOOST_REQUIRE_EQUAL(instance.connections, 0u);
    BOOST_REQUIRE_EQUAL(instance.timeout_seconds, 60u);
    BOOST_REQUIRE(!instance.enabled());

    // http_server
    BOOST_REQUIRE_EQUAL(instance.server, "libbitcoin/4.0");
    BOOST_REQUIRE(instance.hosts.empty());
    BOOST_REQUIRE(instance.host_names().empty());

    // html_server
    BOOST_REQUIRE(instance.path.empty());
    BOOST_REQUIRE_EQUAL(instance.default_, "index.html");
    BOOST_REQUIRE(instance.origins.empty());
    BOOST_REQUIRE(instance.origin_names().empty());
}

BOOST_AUTO_TEST_CASE(server__web_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.web;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "web");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, "libbitcoin/4.0");
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());

    // html_server
    BOOST_REQUIRE(server.path.empty());
    BOOST_REQUIRE_EQUAL(server.default_, "index.html");
    BOOST_REQUIRE(server.origins.empty());
    BOOST_REQUIRE(server.origin_names().empty());
}

BOOST_AUTO_TEST_CASE(server__explore_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.explore;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "explore");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, "libbitcoin/4.0");
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());

    // html_server
    BOOST_REQUIRE(server.path.empty());
    BOOST_REQUIRE_EQUAL(server.default_, "index.html");
    BOOST_REQUIRE(server.origins.empty());
    BOOST_REQUIRE(server.origin_names().empty());
}

BOOST_AUTO_TEST_CASE(server__websocket_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.websocket;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "websocket");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, "libbitcoin/4.0");
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());
}

BOOST_AUTO_TEST_CASE(server__bitcoind_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.bitcoind;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "bitcoind");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, "libbitcoin/4.0");
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());
}

BOOST_AUTO_TEST_CASE(server__electrum_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.electrum;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "electrum");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());
}

BOOST_AUTO_TEST_CASE(server__stratum_v1_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.stratum_v1;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "stratum_v1");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());
}

BOOST_AUTO_TEST_CASE(server__stratum_v2_server__defaults__expected)
{
    const server::settings instance{};
    const auto server = instance.stratum_v2;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "stratum_v2");
    BOOST_REQUIRE(!server.secure);
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.timeout_seconds, 60u);
    BOOST_REQUIRE(!server.enabled());
}

BOOST_AUTO_TEST_SUITE_END()
