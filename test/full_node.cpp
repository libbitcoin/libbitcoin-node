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
#include <chrono>
#include <future>
#include <thread>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A full node without peer connections over a created store.
struct full_node_setup_fixture
{
    DELETE_COPY_MOVE(full_node_setup_fixture);

    using configurator = std::function<void(configuration&)>;
    explicit full_node_setup_fixture(const configurator& configure={})
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
                if (configure)
                    configure(config_);

                return config_.database;
            }()
        },
        query_{ store_ },
        node_{ query_, config_, log_ }
    {
        BOOST_REQUIRE(test::clear(test::directory));
        const auto ec = store_.create([](auto, auto) NOEXCEPT {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    }

    ~full_node_setup_fixture()
    {
        close();
        const auto ec = store_.close([](auto, auto) NOEXCEPT {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    bool initialize()
    {
        return query_.initialize(config_.bitcoin.genesis_block);
    }

    code start()
    {
        std::promise<code> promise{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        });

        return promise.get_future().get();
    }

    code run()
    {
        std::promise<code> promise{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        });

        return promise.get_future().get();
    }

    void close()
    {
        if (!closed_)
            node_.close();

        closed_ = true;
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

    /// Wait (bounded) for the future.
    static bool ready(const std::future<code>& future)
    {
        const auto status = future.wait_for(std::chrono::seconds(10));
        return status == std::future_status::ready;
    }

    /// Wait (bounded) for the condition to be satisfied.
    static bool await(const std::function<bool()>& satisfied)
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

protected:
    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    full_node node_;

private:
    bool closed_{};
};

struct full_node_current_setup_fixture
  : full_node_setup_fixture
{
    inline full_node_current_setup_fixture()
      : full_node_setup_fixture([](configuration& config)
        {
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

struct full_node_maximum_height_setup_fixture
  : full_node_setup_fixture
{
    inline full_node_maximum_height_setup_fixture()
      : full_node_setup_fixture([](configuration& config)
        {
            config.node.maximum_height = 1;
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

BOOST_FIXTURE_TEST_SUITE(full_node_tests, full_node_setup_fixture)

// start/run/close

BOOST_AUTO_TEST_CASE(full_node__start__uninitialized__store_uninitialized)
{
    BOOST_REQUIRE_EQUAL(start(), error::store_uninitialized);
}

BOOST_AUTO_TEST_CASE(full_node__start__invalid_candidate_top__organize1)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(query_.push_candidate(database::header_link{ 42 }));
    BOOST_REQUIRE_EQUAL(start(), error::organize1);
}

BOOST_AUTO_TEST_CASE(full_node__run__started__success)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    BOOST_REQUIRE(!node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__run__closed__service_stopped)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    close();
    BOOST_REQUIRE_EQUAL(run(), network::error::service_stopped);
}

BOOST_AUTO_TEST_CASE(full_node__run__closed_while_queued__service_stopped)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<bool> blocked{};
    boost::asio::post(node_.strand(), [&]() NOEXCEPT
    {
        blocked.set_value(await([&]() { return node_.closed(); }));
    });

    std::promise<code> promise{};
    node_.run([&](const code& ec) NOEXCEPT
    {
        promise.set_value(ec);
    });

    close();
    BOOST_REQUIRE(blocked.get_future().get());
    BOOST_REQUIRE_EQUAL(promise.get_future().get(), network::error::service_stopped);
}

// events

BOOST_AUTO_TEST_CASE(full_node__notify__subscribed__notified)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    auto future = subscribe(chase::template_);
    node_.notify(error::orphan_header, chases::template_{ 42 });
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE_EQUAL(future.get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(full_node__notify_one__subscribed__notified)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<object_key> keyed{};
    std::promise<code> notified{};
    node_.subscribe_chase([&](const code& ec, event_value value) NOEXCEPT
    {
        if (to_chase(value) != chase::template_)
            return true;

        notified.set_value(ec);
        return false;
    }, [&](const code&, object_key key) NOEXCEPT
    {
        keyed.set_value(key);
    });

    node_.notify_one(keyed.get_future().get(), error::orphan_header, chases::template_{ 42 });
    BOOST_REQUIRE_EQUAL(notified.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(full_node__unsubscribe_chase__subscribed__service_stopped)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<object_key> keyed{};
    std::promise<code> stopped{};
    node_.subscribe_chase([&](const code& ec, event_value value) NOEXCEPT
    {
        if (to_chase(value) != chase::stop)
            return true;

        stopped.set_value(ec);
        return false;
    }, [&](const code&, object_key key) NOEXCEPT
    {
        keyed.set_value(key);
    });

    node_.unsubscribe_chase(keyed.get_future().get());
    BOOST_REQUIRE_EQUAL(stopped.get_future().get(), network::error::service_stopped);
}

// suspensions

BOOST_AUTO_TEST_CASE(full_node__fault__node_error__suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto future = subscribe(chase::suspend);
    node_.fault(error::orphan_header);
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE_EQUAL(future.get(), error::suspended_channel);
    BOOST_REQUIRE(node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__resume__suspended__resumed)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    node_.suspend(error::orphan_header);
    BOOST_REQUIRE(node_.suspended());
    auto future = subscribe(chase::resume);
    BOOST_REQUIRE(node_.resume());
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE_EQUAL(future.get(), error::success);
    BOOST_REQUIRE(!node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__snapshot__running__success_suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    BOOST_REQUIRE(!node_.snapshot([](auto, auto) NOEXCEPT {}));
    BOOST_REQUIRE(node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__reload__not_full__success_not_suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    BOOST_REQUIRE_EQUAL(node_.reload([](auto, auto) NOEXCEPT {}), error::success);
    BOOST_REQUIRE(!node_.suspended());
}

// chasers

BOOST_AUTO_TEST_CASE(full_node__notify__snap__resumed)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto future = subscribe(chase::resume);
    node_.notify(error::success, chases::snap{ 0 });
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE_EQUAL(future.get(), error::success);
}

BOOST_AUTO_TEST_CASE(full_node__notify__block__pruned)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto pruned = subscribe(chase::pruned);
    node_.notify(error::success, chases::block{ 0 });
    BOOST_REQUIRE(ready(pruned));
    BOOST_REQUIRE_EQUAL(pruned.get(), error::success);
}

BOOST_AUTO_TEST_CASE(full_node__notify__block_after_prune__not_suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto resumed = subscribe(chase::resume);
    node_.notify(error::success, chases::block{ 0 });
    BOOST_REQUIRE(ready(resumed));
    BOOST_REQUIRE_EQUAL(resumed.get(), error::success);

    auto templated = subscribe(chase::template_);
    node_.notify(error::success, chases::block{ 0 });
    node_.notify(error::success, chases::template_{ 0 });
    BOOST_REQUIRE(ready(templated));
    BOOST_REQUIRE(!node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__notify__space_not_full__not_suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto future = subscribe(chase::template_);
    node_.notify(error::success, chases::space{});
    node_.notify(error::success, chases::template_{ 0 });
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE(!query_.is_full());
    BOOST_REQUIRE(!node_.suspended());
}

BOOST_AUTO_TEST_CASE(full_node__notify__transaction__not_suspended)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());
    BOOST_REQUIRE(!run());
    auto future = subscribe(chase::template_);
    node_.notify(error::success, chases::transaction{ 0 });
    node_.notify(error::success, chases::template_{ 0 });
    BOOST_REQUIRE(ready(future));
    BOOST_REQUIRE(!node_.suspended());
}

// organizers

BOOST_AUTO_TEST_CASE(full_node__organize__orphan__orphan_header)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<code> promise{};
    node_.organize(system::to_shared(orphan), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(full_node__organize__milestone_orphan__orphan_header)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<code> promise{};
    node_.organize(system::to_shared(orphan), true, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::orphan_header);
}

BOOST_AUTO_TEST_CASE(full_node__prioritize__unknown__not_found)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<code> promise{};
    node_.prioritize(orphan.hash(), [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), database::error::not_found);
}

BOOST_AUTO_TEST_CASE(full_node__submit__relay_disabled__pooling_disabled)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!start());

    std::promise<code> promise{};
    const auto txs = system::to_shared<const system::chain::transaction_cptrs>();
    node_.submit(txs, true, [&](const code& ec, size_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::pooling_disabled);
}

BOOST_AUTO_TEST_CASE(full_node__estimate__disabled__estimate_disabled)
{
    std::promise<code> promise{};
    node_.estimate(1, estimator::mode::basic, [&](const code& ec, uint64_t) NOEXCEPT
    {
        promise.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(promise.get_future().get(), error::estimate_disabled);
}

// properties

BOOST_AUTO_TEST_CASE(full_node__start_time__always__not_after_now)
{
    BOOST_REQUIRE_NE(node_.start_time(), 0);
    BOOST_REQUIRE(node_.start_time() <= network::zulu_time());
}

BOOST_AUTO_TEST_CASE(full_node__is_current_header__genesis__false)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!node_.is_current_header(database::header_link{ 0 }));
}

BOOST_AUTO_TEST_CASE(full_node__is_current_header__missing__false)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!node_.is_current_header(database::header_link{ 42 }));
}

BOOST_AUTO_TEST_CASE(full_node__is_recent__genesis__false)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(!node_.is_recent());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(full_node_current_tests, full_node_current_setup_fixture)

BOOST_AUTO_TEST_CASE(full_node__is_current_header__zero_window__true)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(node_.is_current_header(database::header_link{ 42 }));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(full_node_maximum_height_tests, full_node_maximum_height_setup_fixture)

BOOST_AUTO_TEST_CASE(full_node__is_recent__confirmed_to_maximum_height__true)
{
    BOOST_REQUIRE(initialize());
    BOOST_REQUIRE(query_.push_confirmed(database::header_link{ 0 }, false));
    BOOST_REQUIRE(node_.is_recent());
}

BOOST_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
