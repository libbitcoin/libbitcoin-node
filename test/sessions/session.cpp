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

// A running full node without peer connections, and a session over it.
struct session_setup_fixture
{
    DELETE_COPY_MOVE(session_setup_fixture);

    session_setup_fixture()
      : config_{ system::chain::selection::mainnet },
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
        BOOST_REQUIRE(test::clear(test::directory));
        const auto ec = store_.create([](auto, auto) NOEXCEPT {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        BOOST_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        BOOST_REQUIRE(!started.get_future().get());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        BOOST_REQUIRE(!running.get_future().get());
    }

    ~session_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) NOEXCEPT {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
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

static const system::chain::header orphan
{
    1u,
    system::hash_digest{ 0x42 },
    system::null_hash,
    0u,
    0u,
    0u
};

BOOST_FIXTURE_TEST_SUITE(session_tests, session_setup_fixture)

// organizers

BOOST_AUTO_TEST_CASE(session__organize__orphan__orphan_header)
{
    std::promise<code> promise{};
    session_.organize(system::to_shared(orphan), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(session__organize__milestone_orphan__orphan_header)
{
    std::promise<code> promise{};
    session_.organize(system::to_shared(orphan), false, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(session__prioritize__unknown__not_found)
{
    std::promise<code> promise{};
    session_.prioritize(orphan.hash(), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), database::error::not_found);
}

BOOST_AUTO_TEST_CASE(session__submit__relay_disabled__pooling_disabled)
{
    std::promise<code> promise{};
    const auto txs = system::to_shared<const system::chain::transaction_cptrs>();
    session_.submit(txs, false, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::pooling_disabled);
}

BOOST_AUTO_TEST_CASE(session__estimate__disabled__estimate_disabled)
{
    std::promise<code> promise{};
    session_.estimate(1, estimator::mode::basic, [&](const code& ec, uint64_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::estimate_disabled);
}

// events

BOOST_AUTO_TEST_CASE(session__notify_one__subscribed__notified)
{
    std::promise<object_key> keyed{};
    std::promise<code> notified{};
    boost::asio::post(node_.strand(), [&]() NOEXCEPT
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
    BOOST_REQUIRE_EQUAL(notified.get_future().get(), error::orphan_header);
}

// suspensions

BOOST_AUTO_TEST_CASE(session__fault__node_error__suspended)
{
    BOOST_REQUIRE(!session_.suspended());
    session_.fault(error::orphan_header);
    BOOST_REQUIRE(session_.suspended());
}

BOOST_AUTO_TEST_CASE(session__resume__suspended__resumed)
{
    session_.suspend(error::orphan_header);
    BOOST_REQUIRE(session_.suspended());
    BOOST_REQUIRE(session_.resume());
    BOOST_REQUIRE(!session_.suspended());
}

// properties

BOOST_AUTO_TEST_CASE(session__database_settings__always__node_database_settings)
{
    BOOST_REQUIRE_EQUAL(&session_.database_settings(), &node_.database_settings());
}

BOOST_AUTO_TEST_CASE(session__is_current_time__zero__false)
{
    BOOST_REQUIRE(!session_.is_current_time(0));
}

BOOST_AUTO_TEST_CASE(session__start_time__always__node_start_time)
{
    BOOST_REQUIRE_EQUAL(session_.start_time(), node_.start_time());
}

BOOST_AUTO_TEST_CASE(session__channel_counts__no_connections__zero)
{
    BOOST_REQUIRE_EQUAL(session_.channel_count(), 0u);
    BOOST_REQUIRE_EQUAL(session_.inbound_channel_count(), 0u);
    BOOST_REQUIRE_EQUAL(session_.address_count(), 0u);
}

// methods

BOOST_AUTO_TEST_CASE(session__connect__listening_endpoint__version_received)
{
    using namespace network::messages::peer;
    using tcp = boost::asio::ip::tcp;
    boost::asio::io_context io{};
    tcp::acceptor acceptor{ io, tcp::endpoint{ boost::asio::ip::make_address("127.0.0.1"), 65113 } };
    tcp::socket socket{ io };
    session_.connect(network::config::endpoint{ "127.0.0.1:65113" });
    acceptor.accept(socket);

    system::data_array<heading::size()> head{};
    boost::asio::read(socket, boost::asio::buffer(head));
    const auto message = heading::deserialize(head);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->command, version::command);
}

BOOST_AUTO_TEST_CASE(session__connect__refused_endpoint__handler_error)
{
    std::promise<code> promise{};
    session_.connect(network::config::endpoint{ "127.0.0.1:65114" }, [&](const code& ec, const network::channel::ptr&) NOEXCEPT
    {
        promise.set_value(ec);
        return false;
    });

    BOOST_REQUIRE(promise.get_future().get());
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(session_peer_tests, p2p_setup_fixture)

using namespace network::messages::peer;

BOOST_AUTO_TEST_CASE(session_peer__attach_protocols__bip130_network_peer__pong)
{
    BOOST_REQUIRE(handshake(service::node_network, level::bip130));
    send(ping{ 42 }, level::bip130);
    const auto message = pong::deserialize(level::bip130, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_CASE(session_peer__attach_protocols__headers_network_peer__genesis_block)
{
    BOOST_REQUIRE(handshake(service::node_network, level::headers_protocol));

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, level::headers_protocol);
    BOOST_REQUIRE(receive(block::command) == genesis.to_data(false));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(session_peer_relay_tests, p2p_relay_setup_fixture)

using namespace network::messages::peer;

BOOST_AUTO_TEST_CASE(session_peer__attach_protocols__bip37_relay_peer__pong)
{
    BOOST_REQUIRE(handshake(0, level::bip37, true));
    send(ping{ 42 }, level::bip37);
    const auto message = pong::deserialize(level::bip37, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_CASE(session_peer__attach_protocols__bip31_relay_peer__pong)
{
    BOOST_REQUIRE(handshake(0, level::bip31, true));
    send(ping{ 42 }, level::bip31);
    const auto message = pong::deserialize(level::bip31, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
