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
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

class chaser_accessor
  : public chaser
{
public:
    chaser_accessor(full_node& node) NOEXCEPT
      : chaser(node)
    {
    }

    code start() NOEXCEPT override
    {
        return error::success;
    }

    using chaser::fault;
    using chaser::snapshot;
    using chaser::reload;
    using chaser::subscribe_chase;
    using chaser::notify_one;
    using chaser::node_config;
    using chaser::database_settings;
    using chaser::is_current_header;
    using chaser::is_recent;
    using chaser::suspended;
};

// A running full node without peer connections, and a chaser over it.
struct chaser_setup_fixture
{
    DELETE_COPY_MOVE(chaser_setup_fixture);

    chaser_setup_fixture()
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
        chaser_{ node_ }
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

    ~chaser_setup_fixture()
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
    chaser_accessor chaser_;
};

BOOST_FIXTURE_TEST_SUITE(chaser_tests, chaser_setup_fixture)

BOOST_AUTO_TEST_CASE(chaser__fault__node_error__returned_suspended)
{
    BOOST_REQUIRE_EQUAL(chaser_.fault(error::orphan_header), error::orphan_header);
    BOOST_REQUIRE(chaser_.suspended());
}

BOOST_AUTO_TEST_CASE(chaser__snapshot__running__success_suspended)
{
    BOOST_REQUIRE(!chaser_.snapshot([](auto, auto) NOEXCEPT {}));
    BOOST_REQUIRE(chaser_.suspended());
}

BOOST_AUTO_TEST_CASE(chaser__reload__not_full__success)
{
    BOOST_REQUIRE(!chaser_.reload([](auto, auto) NOEXCEPT {}));
    BOOST_REQUIRE(!chaser_.suspended());
}

BOOST_AUTO_TEST_CASE(chaser__notify_one__subscribed__notified)
{
    std::promise<object_key> keyed{};
    std::promise<code> notified{};
    boost::asio::post(node_.strand(), [&]() NOEXCEPT
    {
        keyed.set_value(chaser_.subscribe_chase([&](const code& ec, event_value value) NOEXCEPT
        {
            if (to_chase(value) != chase::template_)
                return true;

            notified.set_value(ec);
            return false;
        }));
    });

    chaser_.notify_one(keyed.get_future().get(), error::orphan_header, chases::template_{ 42 });
    BOOST_REQUIRE_EQUAL(notified.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(chaser__node_config__always__node_config)
{
    BOOST_REQUIRE_EQUAL(&chaser_.node_config(), &config_);
    BOOST_REQUIRE_EQUAL(&chaser_.database_settings(), &config_.database);
}

BOOST_AUTO_TEST_CASE(chaser__is_current_header__genesis__false)
{
    BOOST_REQUIRE(!chaser_.is_current_header(database::header_link{ 0 }));
}

BOOST_AUTO_TEST_CASE(chaser__is_recent__genesis__false)
{
    BOOST_REQUIRE(!chaser_.is_recent());
}

BOOST_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
