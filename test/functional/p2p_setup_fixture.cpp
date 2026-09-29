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
#include "p2p_setup_fixture.hpp"
#include <chrono>
#include <future>
#include <thread>

using namespace bc::system;
using namespace bc::network::messages::peer;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

p2p_setup_fixture::p2p_setup_fixture(const initializer& setup,
    const configurator& configure)
  : config_{ chain::selection::mainnet },
    store_
    {
        [&]() NOEXCEPT -> const database::settings&
        {
            config_.database.path = TEST_DIRECTORY;
            return config_.database;
        }()
    },
    query_{ store_ },
    node_{ query_, config_, log_ }
{
    test::clear(test::directory);

    auto& network_settings = config_.network;
    auto& node_settings = config_.node;

    network_settings.path = TEST_DIRECTORY;
    network_settings.inbound.connections = 1;
    network_settings.inbound.binds.clear();
    network_settings.inbound.binds.emplace_back(P2P_FUNCTIONAL_ENDPOINT);
    network_settings.outbound.connections = 0;
    network_settings.outbound.seeds.clear();
    node_settings.delay_inbound = false;

    // Apply test-specific configuration overrides.
    if (configure)
        configure(config_);

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    BOOST_REQUIRE(query_.initialize(genesis));

    if (setup)
        BOOST_REQUIRE(setup(query_));

    std::promise<code> started{};
    node_.start([&](const code& ec) NOEXCEPT
    {
        started.set_value(ec);
    });

    // Block until the node is started.
    ec = started.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    std::promise<code> running{};
    node_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    // Block until the node is running.
    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    socket_.connect(network_settings.inbound.binds.back().to_endpoint());
}

p2p_setup_fixture::~p2p_setup_fixture()
{
    socket_.close();
    node_.close();
    const auto ec = store_.close([](auto, auto) {});
    BOOST_WARN_MESSAGE(!ec, ec.message());
    test::clear(test::directory);
}

void p2p_setup_fixture::send(const std::string& command,
    const data_chunk& payload)
{
    const auto head = heading::factory(config_.network.identifier, command,
        payload);
    data_chunk frame(heading::size() + payload.size());
    BOOST_REQUIRE(head.serialize({ frame.data(),
        std::next(frame.data(), heading::size()) }));

    std::copy(payload.begin(), payload.end(),
        std::next(frame.begin(), heading::size()));
    boost::asio::write(socket_, boost::asio::buffer(frame));
}

std::pair<std::string, data_chunk> p2p_setup_fixture::receive()
{
    data_array<heading::size()> head_data{};
    boost::asio::read(socket_, boost::asio::buffer(head_data));
    const auto head = heading::deserialize(head_data);
    BOOST_REQUIRE(head);

    data_chunk payload(head->payload_size);
    if (!payload.empty())
        boost::asio::read(socket_, boost::asio::buffer(payload));

    return { head->command, std::move(payload) };
}

data_chunk p2p_setup_fixture::receive(const std::string& command)
{
    while (true)
    {
        auto message = receive();
        if (message.first == command)
            return std::move(message.second);
    }
}

bool p2p_setup_fixture::handshake(uint64_t services, uint32_t value,
    bool relay)
{
    version out{};
    out.value = value;
    out.services = services;
    out.timestamp = sign_cast<uint64_t>(network::zulu_time());
    out.nonce = 42424242;
    out.user_agent = "/test/";
    out.start_height = 0;
    out.relay = relay;
    send(out, value);

    // The node sends its version upon attach and verack upon our version.
    auto got_version = false;
    auto got_acknowledge = false;
    while (!got_version || !got_acknowledge)
    {
        const auto message = receive();
        if (message.first == version::command)
        {
            node_version = version::deserialize(value, message.second);
            if (!node_version)
                return false;

            got_version = true;
        }
        else if (message.first == version_acknowledge::command)
        {
            got_acknowledge = true;
        }
    }

    send(version_acknowledge{}, value);
    return true;
}

system::chain::header p2p_unassociated_setup_fixture::unassociated() NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    return
    {
        1u,
        bitcoin.genesis_block.hash(),
        system::null_hash,
        0u,
        0u,
        0u
    };
}

const system::chain::block& p2p_compact_setup_fixture::block1() NOEXCEPT
{
    static const chain::block instance
    {
        base16_chunk("010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649ffff001d01e362990101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d0104ffffffff0100f2052a0100000043410496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858eeac00000000"),
        true
    };

    return instance;
}

const system::chain::block& p2p_compact_setup_fixture::block2() NOEXCEPT
{
    static const chain::block instance
    {
        base16_chunk("010000004860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000d5fdcc541e25de1c7a5addedf24858b8bb665c9f36ef744ee42c316022c90f9bb0bc6649ffff001d08d2bd610101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d010bffffffff0100f2052a010000004341047211a824f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385237d92167c13e236446b417ab79a0fcae412ae3316b77ac00000000"),
        true
    };

    return instance;
}

bool p2p_compact_setup_fixture::await(const condition& satisfied)
{
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + seconds(10);
    while (steady_clock::now() < deadline)
    {
        if (satisfied())
            return true;

        std::this_thread::sleep_for(milliseconds(10));
    }

    return false;
}

bool p2p_compact_setup_fixture::associated(const hash_digest& hash)
{
    return await([&]()
    {
        return query_.is_associated(query_.to_header(hash));
    });
}

bool p2p_compact_candidate_setup_fixture::candidate(node::query& query) NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const auto& header1 = block1().header();
    const auto& header2 = block2().header();
    const auto work1 = genesis.proof() + header1.proof();
    const auto work2 = work1 + header2.proof();
    const database::context context1{ 0, 1, genesis.timestamp() };
    const database::context context2{ 0, 2, header1.timestamp() };
    return query.set(header1, context1, work1, false) &&
        query.push_candidate(query.to_header(header1.hash())) &&
        query.set(header2, context2, work2, false) &&
        query.push_candidate(query.to_header(header2.hash()));
}

BC_POP_WARNING()
