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
#include "../test.hpp"
#include "../functional/p2p_setup_fixture.hpp"
#include <fstream>
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

class session_accessor
  : public node::session
{
public:
    session_accessor(full_node& node) NOEXCEPT
      : node::session(node)
    {
    }
};

class session_protocol_accessor
  : public node::protocol
{
public:
    explicit session_protocol_accessor(const node::session::ptr& session)
      : node::protocol(session, {})
    {
    }

    using node::protocol::connect;
};

class session_broadcaster
  : public network::session
{
public:
    explicit session_broadcaster(full_node& node) NOEXCEPT
      : network::session(node, 42)
    {
    }

protected:
    void attach_handshake(const network::channel::ptr&,
        network::result_handler&&) NOEXCEPT override
    {
    }

    void attach_protocols(const network::channel::ptr&) NOEXCEPT override
    {
    }

    network::channel::ptr create_channel(
        const network::socket::ptr&) NOEXCEPT override
    {
        return {};
    }
};

// A running full node without peer connections, and a session over it.
struct session_setup_fixture
{
    DELETE_COPY_MOVE(session_setup_fixture);

    session_setup_fixture()
      : config_{ test::node_configuration() },
        store_
        {
            [&]() NOEXCEPT -> const database::settings&
            {
                config_.database.path = TEST_DIRECTORY;
                config_.network.path = TEST_DIRECTORY;
                config_.network.inbound.connections = 0;
                config_.network.outbound.connections = 0;
                config_.network.outbound.seeds.clear();
                return config_.database;
            }()
        },
        query_{ store_ },
        node_{ query_, config_, log_ },
        session_{ node_ }
    {
        BC_REQUIRE(test::clear(test::directory));
        const auto ec = store_.create([](auto, auto) NOEXCEPT {});
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        BC_REQUIRE(!started.get_future().get());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        BC_REQUIRE(!running.get_future().get());
    }

    ~session_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) NOEXCEPT {});
        BC_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

protected:
    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    full_node node_;
    session_accessor session_;
};

// A running full node with one outbound connection to a cached loopback host.
struct session_outbound_setup_fixture
{
    DELETE_COPY_MOVE(session_outbound_setup_fixture);

    session_outbound_setup_fixture()
      : config_{ test::node_configuration() },
        store_
        {
            [&]() NOEXCEPT -> const database::settings&
            {
                config_.database.path = TEST_DIRECTORY;
                config_.network.path = TEST_DIRECTORY;
                config_.network.inbound.connections = 0;
                config_.network.outbound.connections = 1;
                config_.network.outbound.connect_batch_size = 1;
                config_.network.outbound.host_pool_capacity = 1;
                config_.network.outbound.seeds.clear();
                return config_.database;
            }()
        },
        query_{ store_ },
        node_{ query_, config_, log_ }
    {
        using namespace network::messages::peer;
        BC_REQUIRE(test::clear(test::directory));
        const auto ec = store_.create([](auto, auto) NOEXCEPT {});
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        const auto services = service::node_network | service::node_witness;
        const auto port = std::to_string(acceptor_.local_endpoint().port());
        const auto flags = std::to_string(services);
        const auto entry = "127.0.0.1:" + port + "/0/" + flags;
        std::ofstream{ config_.network.file() } << entry << std::endl;

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        BC_REQUIRE(!started.get_future().get());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        BC_REQUIRE(!running.get_future().get());
    }

    ~session_outbound_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) NOEXCEPT {});
        BC_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    /// Accept the outbound connection and read the command of its first message.
    std::string accept()
    {
        using namespace network::messages::peer;
        REQUIRE_SUCCESS(acceptor_, accept, socket_);
        system::data_array<heading::size()> head{};
        test::read(socket_, network::io::buffer(head));
        const auto message = heading::deserialize(head);
        BC_REQUIRE(message);
        return message->command;
    }

    /// Future code of the next notification of the event.
    std::future<code> subscribe(chase event)
    {
        const auto promise = std::make_shared<std::promise<code>>();
        node_.subscribe_chase([=](const code& ec, event_value value) NOEXCEPT
        {
            if (to_chase(value) != event)
                return true;

            promise->set_value(ec);
            return false;
        }, [](const code&, object_key) NOEXCEPT {});

        return promise->get_future();
    }

protected:
    network::io::context io_{};
    network::io::acceptor acceptor_{ io_, test::loopback() };
    network::io::socket socket_{ io_ };
    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    full_node node_;
};

// A running full node with an inbound test peer, and a broadcaster over it.
struct session_broadcast_setup_fixture
  : p2p_setup_fixture
{
    using target = network::diagnostics::target;

    /// Broadcast diagnostics to the target group, capture into the rows.
    code diagnose(const network::diagnostics::rows_ptr& rows, target group)
    {
        std::promise<code> promise{};
        const auto broadcaster = std::make_shared<session_broadcaster>(node_);
        using diagnostics = network::diagnostics;
        const auto handler = [&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        };

        const auto collect = [rows](diagnostics::row&& value) NOEXCEPT
        {
            rows->push_back(std::move(value));
        };

        // The race completes upon release of the last message reference.
        broadcaster->broadcast<diagnostics>(std::make_shared<diagnostics>(std::make_shared<diagnostics::race>(handler), collect, group), 0);
        return promise.get_future().get();
    }

    /// Broadcast a terminator to the identified channel.
    code terminate(uint64_t identifier)
    {
        std::promise<code> promise{};
        const auto broadcaster = std::make_shared<session_broadcaster>(node_);
        using terminator = network::terminator;
        const auto handler = [&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        };

        // The race completes upon match, or release of the last reference.
        broadcaster->broadcast<terminator>(std::make_shared<terminator>(
            std::make_shared<terminator::race>(handler),
            network::error::channel_dropped, identifier), 0);
        return promise.get_future().get();
    }
};

// A node that does not provide blocks.
struct session_peer_blockless_setup_fixture
  : p2p_setup_fixture
{
    inline session_peer_blockless_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.node.provide_blocks = false;
        })
    {
    }
};

// A relaying node with maximum protocol below bip37.
struct session_peer_bip31_relay_setup_fixture
  : p2p_setup_fixture
{
    inline session_peer_bip31_relay_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            using namespace network::messages::peer;
            config.network.enable_relay = true;
            config.network.protocol_maximum = level::bip31;
        })
    {
    }
};

static const system::chain::header orphan
{
    1u,
    system::hash_digest{ 0x42 },
    system::null_hash,
    0u,
    0u,
    0u
};

BC_FIXTURE_TEST_SUITE(session_tests, session_setup_fixture)

// organizers

BC_AUTO_TEST_CASE(session__organize__orphan__orphan_header)
{
    std::promise<code> promise{};
    session_.organize(system::to_shared(orphan), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BC_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BC_AUTO_TEST_CASE(session__organize__milestone_orphan__orphan_header)
{
    std::promise<code> promise{};
    session_.organize(system::to_shared(orphan), false, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BC_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BC_AUTO_TEST_CASE(session__prioritize__unknown__not_found)
{
    std::promise<code> promise{};
    session_.prioritize(orphan.hash(), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BC_REQUIRE_EQUAL(promise.get_future().get(), database::error::not_found);
}

BC_AUTO_TEST_CASE(session__submit__relay_disabled__pooling_disabled)
{
    std::promise<code> promise{};
    const auto txs = system::to_shared<const system::chain::transaction_cptrs>();
    session_.submit(txs, false, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BC_REQUIRE_EQUAL(promise.get_future().get(), error::pooling_disabled);
}

BC_AUTO_TEST_CASE(session__estimate__disabled__estimate_disabled)
{
    std::promise<code> promise{};
    session_.estimate(1, estimate_mode::basic, [&](const code& ec, uint64_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BC_REQUIRE_EQUAL(promise.get_future().get(), error::estimate_disabled);
}

// events

BC_AUTO_TEST_CASE(session__notify_one__subscribed__notified)
{
    std::promise<object_key> keyed{};
    std::promise<code> notified{};
    network::io::post(node_.strand(), [&]() NOEXCEPT
    {
        keyed.set_value(session_.subscribe_chase([&](const code& ec, event_value value) NOEXCEPT
        {
            if (to_chase(value) != chase::template_)
                return true;

            notified.set_value(ec);
            return false;
        }));
    });

    session_.notify_one(keyed.get_future().get(), error::orphan_header, chases::template_{ 42 });
    BC_REQUIRE_EQUAL(notified.get_future().get(), error::orphan_header);
}

// suspensions

BC_AUTO_TEST_CASE(session__fault__node_error__suspended)
{
    BC_REQUIRE(!session_.suspended());
    session_.fault(error::orphan_header);
    BC_REQUIRE(session_.suspended());
}

BC_AUTO_TEST_CASE(session__resume__suspended__resumed)
{
    session_.suspend(error::orphan_header);
    BC_REQUIRE(session_.suspended());
    BC_REQUIRE(session_.resume());
    BC_REQUIRE(!session_.suspended());
}

// properties

BC_AUTO_TEST_CASE(session__database_settings__always__node_database_settings)
{
    BC_REQUIRE_EQUAL(&session_.database_settings(), &node_.database_settings());
}

BC_AUTO_TEST_CASE(session__is_current_time__zero__false)
{
    BC_REQUIRE(!session_.is_current_time(0));
}

BC_AUTO_TEST_CASE(session__start_time__always__node_start_time)
{
    BC_REQUIRE_EQUAL(session_.start_time(), node_.start_time());
}

BC_AUTO_TEST_CASE(session__channel_counts__no_connections__zero)
{
    BC_REQUIRE_EQUAL(session_.channel_count(), 0u);
    BC_REQUIRE_EQUAL(session_.inbound_channel_count(), 0u);
    BC_REQUIRE_EQUAL(session_.address_count(), 0u);
}

// methods

BC_AUTO_TEST_CASE(session__connect__listening_endpoint__version_received)
{
    using namespace network::messages::peer;
    network::io::context io{};
    network::io::acceptor acceptor{ io, test::loopback() };
    network::io::socket socket{ io };
    session_.connect(network::config::endpoint{ acceptor.local_endpoint() });
    REQUIRE_SUCCESS(acceptor, accept, socket);

    system::data_array<heading::size()> head{};
    test::read(socket, network::io::buffer(head));
    const auto message = heading::deserialize(head);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->command, version::command);
}

BC_AUTO_TEST_CASE(session__connect__refused_endpoint__handler_error)
{
    std::promise<code> promise{};
    session_.connect(network::config::endpoint{ test::refuser() }, [&](const code& ec, const network::channel::ptr&) NOEXCEPT
    {
        promise.set_value(ec);
        return false;
    });

    BC_REQUIRE(promise.get_future().get());
}

BC_AUTO_TEST_CASE(protocol__connect__listening_endpoint__version_received)
{
    using namespace network::messages::peer;
    network::io::context io{};
    network::io::acceptor acceptor{ io, test::loopback() };
    network::io::socket socket{ io };
    session_protocol_accessor protocol{ std::make_shared<session_accessor>(node_) };
    protocol.connect(network::config::endpoint{ acceptor.local_endpoint() });
    REQUIRE_SUCCESS(acceptor, accept, socket);

    system::data_array<heading::size()> head{};
    test::read(socket, network::io::buffer(head));
    const auto message = heading::deserialize(head);
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->command, version::command);
}

BC_AUTO_TEST_CASE(protocol__connect__refused_endpoint__handler_error)
{
    std::promise<code> promise{};
    session_protocol_accessor protocol{ std::make_shared<session_accessor>(node_) };
    protocol.connect(network::config::endpoint{ test::refuser() }, [&](const code& ec, const network::channel::ptr&) NOEXCEPT
    {
        promise.set_value(ec);
        return false;
    });

    BC_REQUIRE(promise.get_future().get());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_outbound_tests, session_outbound_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(session_outbound__start__cached_host__version_received)
{
    BC_REQUIRE_EQUAL(accept(), version::command);
}

BC_AUTO_TEST_CASE(session_outbound__handle_chase__stale__handled)
{
    BC_REQUIRE_EQUAL(accept(), version::command);
    auto future = subscribe(chase::template_);
    node_.notify(error::success, chases::stale{});
    node_.notify(error::success, chases::template_{ 0 });
    BC_REQUIRE(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    BC_REQUIRE_EQUAL(future.get(), error::success);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_broadcast_tests, session_broadcast_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(protocol_observer__handle_broadcast_diagnostics__inbound__peer_captured)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    BC_REQUIRE(pong::deserialize(level::bip130, receive(pong::command)));

    const auto rows = std::make_shared<network::diagnostics::rows>();
    BC_REQUIRE(!diagnose(rows, target::inbound));
    BC_REQUIRE_EQUAL(rows->size(), 1u);
    BC_REQUIRE(rows->front().group == target::inbound);
    BC_REQUIRE_EQUAL(rows->front().peer_version, level::bip130);
    BC_REQUIRE_EQUAL(rows->front().peer_services, service::node_network);
    BC_REQUIRE_EQUAL(rows->front().peer_user_agent, "/test/");
}

BC_AUTO_TEST_CASE(protocol_observer__handle_broadcast_diagnostics__manual__none_captured)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    BC_REQUIRE(pong::deserialize(level::bip130, receive(pong::command)));

    const auto rows = std::make_shared<network::diagnostics::rows>();
    BC_REQUIRE(!diagnose(rows, target::manual));
    BC_REQUIRE(rows->empty());
}

BC_AUTO_TEST_CASE(protocol_observer__handle_broadcast_terminator__identified__stopped)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    BC_REQUIRE(pong::deserialize(level::bip130, receive(pong::command)));

    const auto rows = std::make_shared<network::diagnostics::rows>();
    BC_REQUIRE(!diagnose(rows, target::all));
    BC_REQUIRE_EQUAL(rows->size(), 1u);
    BC_REQUIRE(!terminate(rows->front().identifier));
}

BC_AUTO_TEST_CASE(protocol_observer__handle_broadcast_terminator__unidentified__operation_failed)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    BC_REQUIRE(pong::deserialize(level::bip130, receive(pong::command)));

    const auto rows = std::make_shared<network::diagnostics::rows>();
    BC_REQUIRE(!diagnose(rows, target::all));
    BC_REQUIRE_EQUAL(rows->size(), 1u);
    BC_REQUIRE_EQUAL(terminate(add1(rows->front().identifier)), network::error::operation_failed);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_peer_blockless_tests, session_peer_blockless_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(session_peer__attach_protocols__blocks_not_provided__pong)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    const auto message = pong::deserialize(level::bip130, receive(pong::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->nonce, 42u);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_peer_bip31_relay_tests, session_peer_bip31_relay_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(session_peer__attach_protocols__bip37_relay_peer_bip31_node__pong)
{
    BC_REQUIRE(handshake(0, level::bip37, true));
    send(ping{ 42 }, level::bip31);
    const auto message = pong::deserialize(level::bip31, receive(pong::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->nonce, 42u);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_peer_tests, p2p_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(session_peer__attach_protocols__bip130_network_peer__pong)
{
    BC_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    const auto message = pong::deserialize(level::bip130, receive(pong::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->nonce, 42u);
}

BC_AUTO_TEST_CASE(session_peer__attach_protocols__headers_network_peer__genesis_block)
{
    BC_REQUIRE(handshake(service::node_network, level::headers_protocol));

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, level::headers_protocol);
    BC_REQUIRE(receive(block::command) == genesis.to_data(false));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(session_peer_relay_tests, p2p_relay_setup_fixture)

using namespace network::messages::peer;

BC_AUTO_TEST_CASE(session_peer__attach_protocols__bip37_relay_peer__pong)
{
    BC_REQUIRE(handshake(0, level::bip37, true));
    send(ping{ 42 }, level::bip37);
    const auto message = pong::deserialize(level::bip37, receive(pong::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->nonce, 42u);
}

BC_AUTO_TEST_CASE(session_peer__attach_protocols__bip31_relay_peer__pong)
{
    BC_REQUIRE(handshake(0, level::bip31, true));
    send(ping{ 42 }, level::bip31);
    const auto message = pong::deserialize(level::bip31, receive(pong::command));
    BC_REQUIRE(message);
    BC_REQUIRE_EQUAL(message->nonce, 42u);
}

BC_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
