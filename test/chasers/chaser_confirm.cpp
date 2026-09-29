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
#include <chrono>
#include <ctime>
#include <future>
#include <thread>

using namespace system;

static const uint64_t confirm_subsidy = system::settings{ chain::selection::mainnet }.initial_subsidy();

static const chain::block& confirm_genesis()
{
    static const system::settings bitcoin{ chain::selection::mainnet };
    return bitcoin.genesis_block;
}

static chain::transaction confirm_coinbase(uint8_t tag)
{
    const chain::input input{ chain::point{ null_hash, chain::point::null_index }, chain::script{ data_chunk{ 0x01, tag }, false }, max_uint32 };
    return { 1u, chain::inputs{ input }, chain::outputs{ chain::output{ confirm_subsidy, chain::script{ data_chunk{ 0x51 }, false } } }, 0u };
}

static chain::transaction confirm_spend(const chain::block& block)
{
    const chain::input input{ chain::point{ block.transactions_ptr()->front()->hash(false), 0 }, chain::script{}, max_uint32 };
    return { 1u, chain::inputs{ input }, chain::outputs{ chain::output{ confirm_subsidy, chain::script{ data_chunk{ 0x51 }, false } } }, 0u };
}

static chain::block confirm_block(const chain::block& parent, uint32_t height, const chain::transactions& txs)
{
    const auto& genesis = confirm_genesis().header();
    hashes ids{};
    for (const auto& tx: txs)
        ids.push_back(tx.hash(false));

    const chain::header header{ 1u, parent.hash(), sha256::merkle_root(std::move(ids)), genesis.timestamp() + height * 600u, genesis.bits(), height };
    return { header, txs };
}

static const chain::block& confirm_c1()
{
    static const auto instance = confirm_block(confirm_genesis(), 1, { confirm_coinbase(11) });
    return instance;
}

static const chain::block& confirm_c2()
{
    static const auto instance = confirm_block(confirm_c1(), 2, { confirm_coinbase(12) });
    return instance;
}

static const chain::block& confirm_d1()
{
    static const auto instance = confirm_block(confirm_genesis(), 1, { confirm_coinbase(21) });
    return instance;
}

static const chain::block& confirm_d2()
{
    static const auto instance = confirm_block(confirm_d1(), 2, { confirm_coinbase(22) });
    return instance;
}

static const chain::block& confirm_e2()
{
    static const auto instance = confirm_block(confirm_d1(), 2, { confirm_coinbase(23), confirm_spend(confirm_d1()) });
    return instance;
}

static chain::block confirm_recent()
{
    const auto& genesis = confirm_genesis().header();
    const auto coinbase = confirm_coinbase(31);
    const chain::header header{ 1u, genesis.hash(), sha256::merkle_root({ coinbase.hash(false) }), static_cast<uint32_t>(std::time(nullptr) - 55), genesis.bits(), 1u };
    return { header, { coinbase } };
}

static bool confirm_store(node::query& query, const chain::block& block, uint32_t height)
{
    const auto& genesis = confirm_genesis().header();
    const database::context context{ 0, height, genesis.timestamp() };
    return query.set(block, context, genesis.proof() * add1(height), false, false);
}

static bool confirm_candidate(node::query& query, const chain::block& block, uint32_t height)
{
    return confirm_store(query, block, height) && query.push_candidate(query.to_header(block.hash()));
}

static bool confirm_confirmed(node::query& query, const chain::block& block, uint32_t height)
{
    return confirm_store(query, block, height) && query.set_block_confirmable(query.to_header(block.hash())) && query.push_confirmed(query.to_header(block.hash()), true);
}

static bool confirm_stronger(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) && confirm_candidate(query, confirm_d1(), 1) && confirm_candidate(query, confirm_d2(), 2);
}

static bool confirm_weaker(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) && confirm_confirmed(query, confirm_c2(), 2) && confirm_candidate(query, confirm_d1(), 1);
}

static bool confirm_immature(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) && confirm_candidate(query, confirm_e2(), 2);
}

static bool confirm_immature_fork(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) && confirm_immature(query);
}

static bool confirm_candidates(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) && confirm_candidate(query, confirm_d2(), 2);
}

static bool confirm_unfiltered(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) && query.set_block_valid(query.to_header(confirm_d1().hash()));
}

static bool confirm_confirmable(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) && query.set_block_confirmable(query.to_header(confirm_d1().hash()));
}

static bool confirm_confirmables(node::query& query)
{
    return confirm_confirmable(query) && confirm_candidate(query, confirm_d2(), 2) && query.set_block_confirmable(query.to_header(confirm_d2().hash()));
}

static bool confirm_headerless_confirmed(node::query& query)
{
    const auto& genesis = confirm_genesis().header();
    return query.set(confirm_c1().header(), database::context{ 0, 1, genesis.timestamp() }, genesis.proof() * 2u, false) && query.push_confirmed(query.to_header(confirm_c1().hash()), false) && confirm_confirmables(query);
}

static bool confirm_headerless_candidate(node::query& query)
{
    const auto& genesis = confirm_genesis().header();
    return query.set(confirm_d1().header(), database::context{ 0, 1, genesis.timestamp() }, genesis.proof() * 2u, false) && query.push_candidate(query.to_header(confirm_d1().hash())) && query.set_block_confirmable(query.to_header(confirm_d1().hash()));
}

static bool confirm_unheaded(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) && query.push_candidate(query.to_header(confirm_c1().hash())) && confirm_candidate(query, confirm_c2(), 2);
}

static bool confirm_unprevouted(node::query& query)
{
    return confirm_confirmable(query) && confirm_candidate(query, confirm_e2(), 2) && query.set_block_valid(query.to_header(confirm_e2().hash()));
}

static bool confirm_c1_confirmed(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1);
}

static void confirm_uncheckpointed(configuration& config)
{
    config.bitcoin.checkpoints.clear();
}

struct chaser_confirm_setup_fixture
{
    DELETE_COPY_MOVE(chaser_confirm_setup_fixture);

    using condition = std::function<bool()>;
    using initializer = std::function<bool(node::query&)>;
    using configurator = std::function<void(configuration&)>;

    static configuration configure(const configurator& configurer)
    {
        configuration config{ chain::selection::mainnet };
        config.database.path = TEST_DIRECTORY;
        config.network.path = TEST_DIRECTORY;
        config.network.inbound.connections = 0;
        config.network.inbound.binds.clear();
        config.network.outbound.connections = 0;
        config.network.outbound.seeds.clear();
        if (configurer)
            configurer(config);

        return config;
    }

    chaser_confirm_setup_fixture(const initializer& setup, const configurator& configurer)
      : config_{ configure(configurer) }, store_{ config_.database }, query_{ store_ }, node_{ query_, config_, log_ }
    {
        test::clear(test::directory);
        auto ec = store_.create([](auto, auto) {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        BOOST_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));
        BOOST_REQUIRE(setup(query_));

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT { started.set_value(ec); });
        ec = started.get_future().get();
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT { running.set_value(ec); });
        ec = running.get_future().get();
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    }

    ~chaser_confirm_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    full_node node_;

    bool await(const condition& satisfied)
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

    database::header_link link(const chain::block& block) const
    {
        return query_.to_header(block.hash());
    }

    bool confirmed(const chain::block& block, size_t height)
    {
        return await([&]() { return query_.to_confirmed(height) == link(block); });
    }

    bool stated(const chain::block& block, const code& expected)
    {
        return await([&]() { return query_.get_block_state(link(block)) == expected; });
    }
};

struct confirm_stronger_fixture
  : chaser_confirm_setup_fixture
{
    confirm_stronger_fixture()
      : chaser_confirm_setup_fixture(confirm_stronger, confirm_uncheckpointed)
    {
    }
};

struct confirm_weaker_fixture
  : chaser_confirm_setup_fixture
{
    confirm_weaker_fixture()
      : chaser_confirm_setup_fixture(confirm_weaker, confirm_uncheckpointed)
    {
    }
};

struct confirm_immature_fixture
  : chaser_confirm_setup_fixture
{
    confirm_immature_fixture()
      : chaser_confirm_setup_fixture(confirm_immature, confirm_uncheckpointed)
    {
    }
};

struct confirm_immature_fork_fixture
  : chaser_confirm_setup_fixture
{
    confirm_immature_fork_fixture()
      : chaser_confirm_setup_fixture(confirm_immature_fork, confirm_uncheckpointed)
    {
    }
};

struct confirm_current_fixture
  : chaser_confirm_setup_fixture
{
    confirm_current_fixture()
      : chaser_confirm_setup_fixture(confirm_candidates, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.node.currency_window_minutes = 30u * 365u * 24u * 60u;
        })
    {
    }
};

struct confirm_unfiltered_fixture
  : chaser_confirm_setup_fixture
{
    confirm_unfiltered_fixture()
      : chaser_confirm_setup_fixture(confirm_unfiltered, confirm_uncheckpointed)
    {
    }
};

struct confirm_windowed_fixture
  : chaser_confirm_setup_fixture
{
    confirm_windowed_fixture()
      : chaser_confirm_setup_fixture([](node::query&) { return true; }, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.node.currency_window_minutes = 1;
        })
    {
    }

    bool subscribe(const std::shared_ptr<std::promise<void>>& heard, chase awaited)
    {
        const auto subscribed = std::make_shared<std::promise<void>>();
        auto subscription = subscribed->get_future();
        node_.subscribe_chase([heard, awaited](const code&, event_value value) NOEXCEPT
        {
            if (to_chase(value) != awaited)
                return true;

            heard->set_value();
            return false;
        }, [subscribed](const code&, auto) NOEXCEPT
        {
            subscribed->set_value();
        });

        return subscription.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    }
};

struct confirm_confirmable_fixture
  : chaser_confirm_setup_fixture
{
    confirm_confirmable_fixture()
      : chaser_confirm_setup_fixture(confirm_confirmable, confirm_uncheckpointed)
    {
    }
};

struct confirm_c1_confirmed_fixture
  : chaser_confirm_setup_fixture
{
    confirm_c1_confirmed_fixture()
      : chaser_confirm_setup_fixture(confirm_c1_confirmed, confirm_uncheckpointed)
    {
    }
};

struct confirm_headerless_confirmed_fixture
  : chaser_confirm_setup_fixture
{
    confirm_headerless_confirmed_fixture()
      : chaser_confirm_setup_fixture(confirm_headerless_confirmed, confirm_uncheckpointed)
    {
    }
};

struct confirm_headerless_candidate_fixture
  : chaser_confirm_setup_fixture
{
    confirm_headerless_candidate_fixture()
      : chaser_confirm_setup_fixture(confirm_headerless_candidate, confirm_uncheckpointed)
    {
    }
};

struct confirm_unheaded_fixture
  : chaser_confirm_setup_fixture
{
    confirm_unheaded_fixture()
      : chaser_confirm_setup_fixture(confirm_unheaded, {})
    {
    }
};

struct confirm_unprevouted_fixture
  : chaser_confirm_setup_fixture
{
    confirm_unprevouted_fixture()
      : chaser_confirm_setup_fixture(confirm_unprevouted, confirm_uncheckpointed)
    {
    }
};

BOOST_AUTO_TEST_SUITE(chaser_confirm_tests)

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__stronger_candidate_fork__reorganized, confirm_stronger_fixture)
{
    BOOST_REQUIRE(confirmed(confirm_d2(), 2));
    BOOST_REQUIRE(confirmed(confirm_d1(), 1));
    BOOST_REQUIRE(stated(confirm_d2(), database::error::block_confirmable));
    BOOST_REQUIRE(!query_.is_confirmed_block(link(confirm_c1())));
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__weaker_candidate_fork__not_reorganized, confirm_weaker_fixture)
{
    BOOST_REQUIRE(stated(confirm_d1(), database::error::block_valid));
    BOOST_REQUIRE(confirmed(confirm_c2(), 2));
    BOOST_REQUIRE(confirmed(confirm_c1(), 1));
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__immature_spend__unconfirmable, confirm_immature_fixture)
{
    BOOST_REQUIRE(stated(confirm_e2(), database::error::block_unconfirmable));
    BOOST_REQUIRE(confirmed(confirm_d1(), 1));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__immature_spend_fork__unconfirmable, confirm_immature_fork_fixture)
{
    BOOST_REQUIRE(stated(confirm_e2(), database::error::block_unconfirmable));
    BOOST_REQUIRE(await([&]() { return query_.get_top_confirmed() == 1u; }));
    BOOST_REQUIRE(!query_.is_confirmed_block(link(confirm_e2())));
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__current_candidates__organized, confirm_current_fixture)
{
    BOOST_REQUIRE(confirmed(confirm_d2(), 2));
    BOOST_REQUIRE(stated(confirm_d2(), database::error::block_confirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__valid_candidate_without_filter_body__suspended, confirm_unfiltered_fixture)
{
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE(stated(confirm_d1(), database::error::block_valid));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__bump__recent_confirmable_candidate__organized_stale, confirm_windowed_fixture)
{
    const auto block = confirm_recent();
    const auto heard = std::make_shared<std::promise<void>>();
    auto hearing = heard->get_future();
    BOOST_REQUIRE(subscribe(heard, chase::stale));
    BOOST_REQUIRE(confirm_store(query_, block, 1) && query_.push_candidate(link(block)) && query_.set_block_confirmable(link(block)));
    node_.notify(node::error::success, chases::bump{ 0 });
    BOOST_REQUIRE(confirmed(block, 1));
    BOOST_REQUIRE(hearing.wait_for(std::chrono::seconds(30)) == std::future_status::ready);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__confirmable_candidate__organized, confirm_confirmable_fixture)
{
    BOOST_REQUIRE(confirmed(confirm_d1(), 1));
    BOOST_REQUIRE(!node_.suspended());
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__bump__unstored_confirmed_top__suspended, confirm_c1_confirmed_fixture)
{
    BOOST_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
    BOOST_REQUIRE(confirm_candidate(query_, confirm_d1(), 1));
    BOOST_REQUIRE(query_.set_block_valid(link(confirm_d1())));
    node_.notify(node::error::success, chases::bump{ 0 });
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE(confirmed(confirm_c1(), 1));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), 2u);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__headerless_confirmed_stronger_fork__suspended, confirm_headerless_confirmed_fixture)
{
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE(confirmed(confirm_c1(), 1));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__headerless_confirmable_candidate__suspended, confirm_headerless_candidate_fixture)
{
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__bypassed_candidate_unheaded_parent__suspended, confirm_unheaded_fixture)
{
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE(confirmed(confirm_c1(), 1));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_confirm__start__valid_candidate_without_prevouts__suspended_unconfirmable, confirm_unprevouted_fixture)
{
    BOOST_REQUIRE(await([&]() { return node_.suspended(); }));
    BOOST_REQUIRE(stated(confirm_e2(), database::error::block_unconfirmable));
    BOOST_REQUIRE(await([&]() { return query_.get_top_confirmed() == zero; }));
}

BOOST_AUTO_TEST_SUITE_END()
