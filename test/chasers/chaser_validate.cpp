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
#include <future>
#include <thread>

using namespace system;

static const uint64_t validate_subsidy = system::settings{ chain::selection::mainnet }.initial_subsidy();

static const ec_compressed validate_generator = base16_array("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
static const ec_xonly validate_generator_xonly = base16_array("79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");

static const chain::block& validate_genesis()
{
    static const system::settings bitcoin{ chain::selection::mainnet };
    return bitcoin.genesis_block;
}

static chain::transaction validate_coinbase(uint8_t tag, uint64_t value)
{
    const chain::input input{ chain::point{ null_hash, chain::point::null_index }, chain::script{ data_chunk{ 0x01, tag }, false }, max_uint32 };
    return { 1u, chain::inputs{ input }, chain::outputs{ chain::output{ value, chain::script{ data_chunk{ 0x51 }, false } } }, 0u };
}

static chain::transaction validate_spend(const chain::point& point, uint64_t value)
{
    const chain::input input{ point, chain::script{}, max_uint32 };
    return { 1u, chain::inputs{ input }, chain::outputs{ chain::output{ value, chain::script{ data_chunk{ 0x51 }, false } } }, 0u };
}

static chain::block validate_block(const chain::block& parent, uint32_t height, const chain::transactions& txs)
{
    const auto& genesis = validate_genesis().header();
    hashes ids{};
    for (const auto& tx: txs)
        ids.push_back(tx.hash(false));

    const chain::header header{ 1u, parent.hash(), sha256::merkle_root(std::move(ids)), genesis.timestamp() + height * 600u, genesis.bits(), height };
    return { header, txs };
}

static const chain::block& validate_a1()
{
    static const auto instance = validate_block(validate_genesis(), 1, { validate_coinbase(1, validate_subsidy) });
    return instance;
}

static const chain::block& validate_a2()
{
    static const auto instance = validate_block(validate_a1(), 2, { validate_coinbase(2, validate_subsidy) });
    return instance;
}

static const chain::block& validate_overspent1()
{
    static const auto instance = validate_block(validate_genesis(), 1, { validate_coinbase(3, add1(validate_subsidy)) });
    return instance;
}

static const chain::block& validate_missing2()
{
    static const chain::point missing{ validate_a1().transactions_ptr()->front()->hash(false), 7 };
    static const auto instance = validate_block(validate_a1(), 2, { validate_coinbase(4, validate_subsidy), validate_spend(missing, validate_subsidy) });
    return instance;
}

static const chain::block& validate_x1()
{
    static const auto instance = validate_block(validate_genesis(), 1, { validate_coinbase(5, validate_subsidy) });
    return instance;
}

static const chain::block& validate_a3()
{
    static const auto instance = validate_block(validate_a2(), 3, { validate_coinbase(6, validate_subsidy) });
    return instance;
}

static const chain::block& validate_immature1()
{
    static const auto coinbase = validate_coinbase(7, validate_subsidy);
    static const auto instance = validate_block(validate_genesis(), 1, { coinbase, validate_spend(chain::point{ coinbase.hash(false), 0 }, validate_subsidy) });
    return instance;
}

static const chain::block& validate_coinbases1()
{
    static const auto instance = validate_block(validate_genesis(), 1, { validate_coinbase(8, validate_subsidy), validate_coinbase(9, validate_subsidy) });
    return instance;
}

static bool validate_store(node::query& query, const chain::block& block, uint32_t height)
{
    const auto& genesis = validate_genesis().header();
    const database::context context{ 0, height, genesis.timestamp() };
    return query.set(block, context, genesis.proof() * add1(height), false, false);
}

static bool validate_candidate(node::query& query, const chain::block& block, uint32_t height)
{
    return validate_store(query, block, height) && query.push_candidate(query.to_header(block.hash()));
}

static bool validate_candidates(node::query& query)
{
    return validate_candidate(query, validate_a1(), 1) && validate_candidate(query, validate_a2(), 2);
}

static bool validate_overspent(node::query& query)
{
    return validate_candidate(query, validate_overspent1(), 1);
}

static bool validate_missing(node::query& query)
{
    return validate_candidate(query, validate_a1(), 1) && validate_candidate(query, validate_missing2(), 2);
}

static bool validate_prevalidated(node::query& query)
{
    return validate_candidates(query) && query.set_block_valid(query.to_header(validate_a1().hash())) && query.set_block_unconfirmable(query.to_header(validate_a2().hash()));
}

static bool validate_noncandidate(node::query& query)
{
    return validate_store(query, validate_x1(), 1);
}

static bool validate_staged(node::query& query)
{
    return validate_noncandidate(query) && query.set_prevalid(query.to_header(validate_x1().hash()));
}

static bool validate_immature(node::query& query)
{
    return validate_candidate(query, validate_immature1(), 1);
}

static bool validate_coinbases(node::query& query)
{
    return validate_candidate(query, validate_coinbases1(), 1);
}

static void validate_uncheckpointed(configuration& config)
{
    config.bitcoin.checkpoints.clear();
}

static void validate_current(configuration& config)
{
    config.bitcoin.checkpoints.clear();
    config.node.currency_window_minutes = 0;
}

struct chaser_validate_setup_fixture
{
    DELETE_COPY_MOVE(chaser_validate_setup_fixture);

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

    chaser_validate_setup_fixture(const initializer& setup, const configurator& configurer)
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

    ~chaser_validate_setup_fixture()
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

    code state(const chain::block& block) const
    {
        return query_.get_block_state(query_.to_header(block.hash()));
    }

    bool confirmed(size_t top)
    {
        return await([&]() { return query_.get_top_confirmed() == top; });
    }

    bool stated(const chain::block& block, const code& expected)
    {
        return await([&]() { return state(block) == expected; });
    }
};

struct validate_unvalidated_fixture
  : chaser_validate_setup_fixture
{
    validate_unvalidated_fixture()
      : chaser_validate_setup_fixture(validate_candidates, validate_uncheckpointed)
    {
    }
};

struct validate_current_fixture
  : chaser_validate_setup_fixture
{
    validate_current_fixture()
      : chaser_validate_setup_fixture(validate_candidates, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.node.currency_window_minutes = 0;
            config.node.silent_start_height = 0;
        })
    {
    }
};

struct validate_overspent_fixture
  : chaser_validate_setup_fixture
{
    validate_overspent_fixture()
      : chaser_validate_setup_fixture(validate_overspent, validate_uncheckpointed)
    {
    }
};

struct validate_missing_fixture
  : chaser_validate_setup_fixture
{
    validate_missing_fixture()
      : chaser_validate_setup_fixture(validate_missing, validate_uncheckpointed)
    {
    }
};

struct validate_checkpointed_fixture
  : chaser_validate_setup_fixture
{
    validate_checkpointed_fixture()
      : chaser_validate_setup_fixture(validate_candidates, {})
    {
    }
};

struct validate_unfiltered_fixture
  : chaser_validate_setup_fixture
{
    validate_unfiltered_fixture()
      : chaser_validate_setup_fixture(validate_candidates, [](configuration& config)
        {
            config.database.filter_bk.buckets = 0;
            config.database.filter_tx.buckets = 0;
        })
    {
    }
};

struct validate_prevalidated_fixture
  : chaser_validate_setup_fixture
{
    validate_prevalidated_fixture()
      : chaser_validate_setup_fixture(validate_prevalidated, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.database.filter_bk.buckets = 0;
            config.database.filter_tx.buckets = 0;
        })
    {
    }
};

struct validate_maximum_fixture
  : chaser_validate_setup_fixture
{
    validate_maximum_fixture()
      : chaser_validate_setup_fixture(validate_candidates, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.node.maximum_height = 1;
        })
    {
    }
};

struct validate_staged_fixture
  : chaser_validate_setup_fixture
{
    validate_staged_fixture()
      : chaser_validate_setup_fixture(validate_staged, [](configuration& config)
        {
            config.node.batch_signatures = 1;
        })
    {
    }
};

struct validate_windowed_fixture
  : chaser_validate_setup_fixture
{
    validate_windowed_fixture()
      : chaser_validate_setup_fixture(validate_noncandidate, {})
    {
    }
};


struct validate_immature_fixture
  : chaser_validate_setup_fixture
{
    validate_immature_fixture()
      : chaser_validate_setup_fixture(validate_immature, validate_uncheckpointed)
    {
    }
};

struct validate_coinbases_fixture
  : chaser_validate_setup_fixture
{
    validate_coinbases_fixture()
      : chaser_validate_setup_fixture(validate_coinbases, validate_uncheckpointed)
    {
    }
};

struct validate_current_overspent_fixture
  : chaser_validate_setup_fixture
{
    validate_current_overspent_fixture()
      : chaser_validate_setup_fixture(validate_overspent, validate_current)
    {
    }
};


BOOST_AUTO_TEST_SUITE(chaser_validate_tests)

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__unvalidated_candidates__confirmable, validate_unvalidated_fixture)
{
    BOOST_REQUIRE(confirmed(2));
    BOOST_REQUIRE(stated(validate_a1(), database::error::block_confirmable));
    BOOST_REQUIRE(stated(validate_a2(), database::error::block_confirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__current_candidates__confirmable, validate_current_fixture)
{
    BOOST_REQUIRE(confirmed(2));
    BOOST_REQUIRE(stated(validate_a1(), database::error::block_confirmable));
    BOOST_REQUIRE(stated(validate_a2(), database::error::block_confirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__overspent_coinbase__unconfirmable, validate_overspent_fixture)
{
    BOOST_REQUIRE(stated(validate_overspent1(), database::error::block_unconfirmable));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__missing_prevout__unconfirmable, validate_missing_fixture)
{
    BOOST_REQUIRE(stated(validate_missing2(), database::error::block_unconfirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__checkpointed_candidates__organized, validate_checkpointed_fixture)
{
    BOOST_REQUIRE(confirmed(2));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__checkpointed_unfiltered_candidates__organized, validate_unfiltered_fixture)
{
    BOOST_REQUIRE(confirmed(2));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__prevalidated_candidates__valid_organized, validate_prevalidated_fixture)
{
    BOOST_REQUIRE(confirmed(1));
    BOOST_REQUIRE(stated(validate_a1(), database::error::block_confirmable));
    BOOST_REQUIRE(stated(validate_a2(), database::error::block_unconfirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__maximum_height__confirmable, validate_maximum_fixture)
{
    BOOST_REQUIRE(stated(validate_a1(), database::error::block_confirmable));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__staged_prevalid_unbatched__purged, validate_staged_fixture)
{
    BOOST_REQUIRE_EQUAL(query_.prevalid_records(), zero);
    BOOST_REQUIRE(state(validate_x1()) != database::error::block_valid);
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__windowed__prevalid__valid, validate_windowed_fixture)
{
    BOOST_REQUIRE(query_.set_prevalid(query_.to_header(validate_x1().hash())));
    node_.notify(node::error::success, chases::windowed{ 1 });
    BOOST_REQUIRE(stated(validate_x1(), database::error::block_valid));
    BOOST_REQUIRE(await([&]() { return is_zero(query_.prevalid_records()); }));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__windowed__invalid_ecdsa_signature__unconfirmable, validate_windowed_fixture)
{
    const auto link = query_.to_header(validate_x1().hash());
    chain::ecdsa_signatures signatures{};
    BOOST_REQUIRE(signatures.append(null_hash, validate_generator, ec_signature{}));
    BOOST_REQUIRE(query_.set_signatures(signatures, link));
    BOOST_REQUIRE(query_.set_prevalid(link));
    node_.notify(node::error::success, chases::windowed{ 1 });
    BOOST_REQUIRE(stated(validate_x1(), database::error::block_unconfirmable));
    BOOST_REQUIRE(await([&]() { return is_zero(query_.ecdsa_records()) && is_zero(query_.prevalid_records()); }));
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__windowed__invalid_schnorr_signature__unconfirmable, validate_windowed_fixture)
{
    const auto link = query_.to_header(validate_x1().hash());
    chain::schnorr_signatures signatures{};
    signatures.append(null_hash, validate_generator_xonly, ec_signature{});
    BOOST_REQUIRE(query_.set_signatures(signatures, link));
    BOOST_REQUIRE(query_.set_prevalid(link));
    node_.notify(node::error::success, chases::windowed{ 1 });
    BOOST_REQUIRE(stated(validate_x1(), database::error::block_unconfirmable));
    BOOST_REQUIRE(await([&]() { return is_zero(query_.schnorr_records()) && is_zero(query_.prevalid_records()); }));
}

////BOOST_FIXTURE_TEST_CASE(chaser_validate__regressed__confirmed_candidates__unchanged, validate_unvalidated_fixture)
////{
////    BOOST_REQUIRE(confirmed(2));
////    node_.notify(node::error::success, chases::unfull{});
////    node_.notify(node::error::success, chases::regressed{ 0 });
////    node_.notify(node::error::success, chases::disorganized{ 0 });
////    node_.notify(node::error::success, chases::bump{ 0 });
////    node_.notify(node::error::success, chases::checked{ 1 });
////    BOOST_REQUIRE(stated(validate_a2(), database::error::block_confirmable));
////    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), 2u);
////}

BOOST_FIXTURE_TEST_CASE(chaser_validate__bump__unfull_new_candidate__confirmable, validate_unvalidated_fixture)
{
    BOOST_REQUIRE(confirmed(2));
    BOOST_REQUIRE(validate_candidate(query_, validate_a3(), 3));
    node_.notify(node::error::success, chases::unfull{});
    node_.notify(node::error::success, chases::bump{ 0 });
    BOOST_REQUIRE(stated(validate_a3(), database::error::block_confirmable));
    BOOST_REQUIRE(confirmed(3));
}


BOOST_FIXTURE_TEST_CASE(chaser_validate__start__immature_internal_spend__unconfirmable, validate_immature_fixture)
{
    BOOST_REQUIRE(stated(validate_immature1(), database::error::block_unconfirmable));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__extra_coinbases__unconfirmable, validate_coinbases_fixture)
{
    BOOST_REQUIRE(stated(validate_coinbases1(), database::error::block_unconfirmable));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_validate__start__current_overspent_coinbase__unconfirmable, validate_current_overspent_fixture)
{
    BOOST_REQUIRE(stated(validate_overspent1(), database::error::block_unconfirmable));
    BOOST_REQUIRE_EQUAL(query_.get_top_confirmed(), zero);
}


BOOST_AUTO_TEST_SUITE_END()
