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
#include <memory>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

static const uint32_t genesis_time = 1231006505;
static const uint32_t difficulty_one = 0x1d00ffff;
static const auto await_limit = std::chrono::seconds(10);

static const system::settings& mainnet() NOEXCEPT
{
    static const system::settings instance{ system::chain::selection::mainnet };
    return instance;
}

static const system::hash_digest& genesis_hash() NOEXCEPT
{
    static const auto hash = mainnet().genesis_block.hash();
    return hash;
}

// A distinct difficulty one header that does not satisfy its proof of work.
static system::chain::header::cptr make(const system::hash_digest& previous,
    uint32_t nonce, uint32_t timestamp=add1(genesis_time), uint32_t version=1)
{
    return std::make_shared<const system::chain::header>(version, previous,
        system::null_hash, timestamp, difficulty_one, nonce);
}

// Mainnet block 1 header.
static system::chain::header::cptr block1()
{
    const auto data = system::base16_chunk
    (
        "010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d61900"
        "00000000982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e8"
        "57233e0e61bc6649ffff001d01e36299"
    );
    return std::make_shared<const system::chain::header>(data);
}

// Cumulative work of a difficulty one chain of count headers (genesis is one).
static uint256_t work(size_t count)
{
    return uint256_t{ count } * system::chain::header::proof(difficulty_one);
}

struct chaser_header_setup_fixture
{
    DELETE_COPY_MOVE(chaser_header_setup_fixture);

    using configurator = std::function<void(configuration&)>;
    using result = std::pair<code, size_t>;

    explicit chaser_header_setup_fixture(const configurator& configure={})
      : config_{ configured(configure) },
        store_{ config_.database },
        query_{ store_ },
        node_{ query_, config_, log_ }
    {
        test::clear(test::directory);
        auto ec = store_.create([](auto, auto) {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        BOOST_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        ec = started.get_future().get();
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        ec = running.get_future().get();
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    }

    ~chaser_header_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    static configuration configured(const configurator& configure)
    {
        configuration config{ system::chain::selection::mainnet };
        config.database.path = TEST_DIRECTORY;
        config.network.path = TEST_DIRECTORY;
        config.network.inbound.connections = 0;
        config.network.inbound.binds.clear();
        config.network.outbound.connections = 0;
        config.network.outbound.seeds.clear();
        config.bitcoin.checkpoints.clear();

        if (configure)
            configure(config);

        return config;
    }

    /// Organize an unproven header.
    result organize(const system::chain::header::cptr& header)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        node_.organize(header, [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        });

        BOOST_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Organize a proven header.
    result organize(const system::chain::header::cptr& header, bool milestone)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        const auto handler = [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        };

        node_.organize(header, milestone, handler);

        BOOST_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Prioritize a cached branch.
    result prioritize(const system::hash_digest& hash)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        node_.prioritize(hash, [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        });

        BOOST_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Complete all work posted to the header chaser before this call.
    void flush()
    {
        const auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        node_.get_minimum_work([promise](const code&, const uint256_t&) NOEXCEPT
        {
            promise->set_value();
        });

        BOOST_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
    }

    /// Notify the event and wait (bounded) for the awaited event.
    bool notify(const event_value& event, chase awaited)
    {
        const auto subscribed = std::make_shared<std::promise<void>>();
        const auto heard = std::make_shared<std::promise<void>>();
        auto subscription = subscribed->get_future();
        auto hearing = heard->get_future();

        node_.subscribe_chase([heard, awaited](const code& ec, event_value value) NOEXCEPT
        {
            if (ec)
                return false;

            if (to_chase(value) != awaited)
                return true;

            heard->set_value();
            return false;
        }, [subscribed](const code&, auto) NOEXCEPT
        {
            subscribed->set_value();
        });

        if (subscription.wait_for(await_limit) != std::future_status::ready)
            return false;

        node_.notify(error::success, event);
        return hearing.wait_for(await_limit) == std::future_status::ready;
    }

    header_t link(const system::hash_digest& hash) const
    {
        return query_.to_header(hash).value;
    }

    bool candidate(size_t height, const system::hash_digest& hash) const
    {
        return query_.to_candidate(height) == query_.to_header(hash);
    }

    bool unconfirmable(const system::hash_digest& hash) const
    {
        return query_.is_unconfirmable(query_.to_header(hash));
    }

    /// Archive a header directly, bypassing the chaser.
    bool archive(const system::chain::header& header, uint32_t height,
        size_t count)
    {
        const database::context context{ 0, height, header.timestamp() };
        return query_.set(header, context, work(count), false);
    }

    /// Archive and push a candidate directly, bypassing the chaser.
    bool push(const system::chain::header& header, uint32_t height,
        size_t count)
    {
        return archive(header, height, count) &&
            query_.push_candidate(query_.to_header(header.hash()));
    }

protected:
    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    full_node node_;
};

struct chaser_header_window_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_window_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            config.node.currency_window_minutes = 10;
        })
    {
    }
};

struct chaser_header_conflict_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_conflict_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            const auto hash = make(genesis_hash(), 2)->hash();
            config.bitcoin.checkpoints.emplace_back(hash, 1);
        })
    {
    }
};

struct chaser_header_checkpoint_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_checkpoint_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            const auto parent = make(genesis_hash(), 1)->hash();
            config.bitcoin.checkpoints.emplace_back(make(parent, 3)->hash(), 2);
        })
    {
    }
};

struct chaser_header_fork_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_fork_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            config.bitcoin.bip90_bip34_height = 1;
        })
    {
    }
};

BOOST_AUTO_TEST_SUITE(chaser_header_tests)

// validate
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__validate__block1__success, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BOOST_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BOOST_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), error::success);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__validate__unsatisfied_work__invalid_proof_of_work, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BOOST_REQUIRE(genesis);

    const auto header = make(genesis_hash(), 1);
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BOOST_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), system::error::invalid_proof_of_work);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__validate__version_under_bip34_minimum__insufficient_block_version, chaser_header_fork_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BOOST_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BOOST_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), system::error::insufficient_block_version);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__validate__checkpoint_hash_mismatch__checkpoint_conflict, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BOOST_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    system::settings bitcoin{ config_.bitcoin };
    bitcoin.checkpoints.emplace_back(system::null_hash, 1);
    BOOST_REQUIRE_EQUAL(chaser_header::validate(*header, state, bitcoin), system::error::checkpoint_conflict);
}

// organize (unproven)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__organize__block1__candidate, chaser_header_setup_fixture)
{
    const auto header = block1();
    const auto result = organize(header);
    BOOST_REQUIRE_EQUAL(result.first, error::success);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BOOST_REQUIRE(candidate(1, header->hash()));
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize__archived__duplicate_header, chaser_header_setup_fixture)
{
    BOOST_REQUIRE_EQUAL(organize(block1()).first, error::success);

    const auto result = organize(block1());
    BOOST_REQUIRE_EQUAL(result.first, error::duplicate_header);
    BOOST_REQUIRE_EQUAL(result.second, max_size_t);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize__unsatisfied_work__invalid_proof_of_work, chaser_header_setup_fixture)
{
    const auto result = organize(make(genesis_hash(), 1));
    BOOST_REQUIRE_EQUAL(result.first, system::error::invalid_proof_of_work);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize__unknown_parent__orphan_header, chaser_header_setup_fixture)
{
    const auto result = organize(make(system::one_hash, 1));
    BOOST_REQUIRE_EQUAL(result.first, error::orphan_header);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

// organize (proven)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__unsatisfied_work__candidate, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto result = organize(a1, false);
    BOOST_REQUIRE_EQUAL(result.first, error::success);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE(candidate(1, a1->hash()));
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__unconfirmable_parent__block_unconfirmable, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE(query_.set_block_unconfirmable(query_.to_header(a1->hash())));

    const auto result = organize(make(a1->hash(), 2), false);
    BOOST_REQUIRE_EQUAL(result.first, database::error::block_unconfirmable);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__archived_unconfirmable__block_unconfirmable_height, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE(query_.set_block_unconfirmable(query_.to_header(a1->hash())));

    const auto result = organize(make(genesis_hash(), 1), false);
    BOOST_REQUIRE_EQUAL(result.first, database::error::block_unconfirmable);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__equal_work_branch__cached_not_archived, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);

    const auto result = organize(b1, false);
    BOOST_REQUIRE_EQUAL(result.first, error::success);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE(candidate(1, a1->hash()));
    BOOST_REQUIRE(query_.to_header(b1->hash()).is_terminal());

    const auto duplicate = organize(make(genesis_hash(), 2), false);
    BOOST_REQUIRE_EQUAL(duplicate.first, error::duplicate_header);
    BOOST_REQUIRE_EQUAL(duplicate.second, 1u);
}

////BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__stronger_cached_branch__reorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto b1 = make(genesis_hash(), 2);
////    const auto b2 = make(b1->hash(), 3);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);

////    const auto result = organize(b2, false);
////    BOOST_REQUIRE_EQUAL(result.first, error::success);
////    BOOST_REQUIRE_EQUAL(result.second, 2u);
////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
////    BOOST_REQUIRE(candidate(1, b1->hash()));
////    BOOST_REQUIRE(candidate(2, b2->hash()));
////    BOOST_REQUIRE(!query_.is_candidate_header(query_.to_header(a1->hash())));
////}

////BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__stronger_archived_branch__reorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto a2 = make(a1->hash(), 4);
////    const auto a3 = make(a2->hash(), 5);
////    const auto b1 = make(genesis_hash(), 2);
////    const auto b2 = make(b1->hash(), 3);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b2, false).first, error::success);
////    BOOST_REQUIRE(candidate(2, b2->hash()));

////    const auto weak = organize(a2, false);
////    BOOST_REQUIRE_EQUAL(weak.first, error::success);
////    BOOST_REQUIRE_EQUAL(weak.second, 2u);
////    BOOST_REQUIRE(candidate(2, b2->hash()));

////    const auto result = organize(a3, false);
////    BOOST_REQUIRE_EQUAL(result.first, error::success);
////    BOOST_REQUIRE_EQUAL(result.second, 3u);
////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 3u);
////    BOOST_REQUIRE(candidate(1, a1->hash()));
////    BOOST_REQUIRE(candidate(2, a2->hash()));
////    BOOST_REQUIRE(candidate(3, a3->hash()));
////}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__fork_activation__candidate, chaser_header_fork_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1, config_.bitcoin.bip16_activation_time, config_.bitcoin.bip34_version);
    const auto result = organize(a1, false);
    BOOST_REQUIRE_EQUAL(result.first, error::success);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE(candidate(1, a1->hash()));
}

// organize (store faults)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__candidate_above_chaser_top__organize4, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto x2 = make(a1->hash(), 2);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE(push(*x2, 2, 3));

    const auto result = organize(make(x2->hash(), 3), false);
    BOOST_REQUIRE_EQUAL(result.first, error::organize4);
    BOOST_REQUIRE_EQUAL(result.second, 3u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__candidate_below_chaser_top__organize5, chaser_header_setup_fixture)
{
    BOOST_REQUIRE_EQUAL(organize(make(genesis_hash(), 1), false).first, error::success);
    BOOST_REQUIRE(query_.pop_candidate());

    const auto result = organize(make(genesis_hash(), 2), false);
    BOOST_REQUIRE_EQUAL(result.first, error::organize5);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__unstored_candidate_top__organize3, chaser_header_setup_fixture)
{
    BOOST_REQUIRE_EQUAL(organize(make(genesis_hash(), 1), false).first, error::success);
    BOOST_REQUIRE(query_.push_candidate(database::header_link{ 42 }));

    const auto result = organize(make(genesis_hash(), 2), false);
    BOOST_REQUIRE_EQUAL(result.first, error::organize3);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
}

// checkpoints
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__checkpoint_hash_mismatch__checkpoint_conflict, chaser_header_conflict_setup_fixture)
{
    const auto result = organize(make(genesis_hash(), 1), false);
    BOOST_REQUIRE_EQUAL(result.first, system::error::checkpoint_conflict);
    BOOST_REQUIRE_EQUAL(result.second, 1u);
    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__under_reached_checkpoint__checkpoint_conflict, chaser_header_checkpoint_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 3);
    const auto b1 = make(genesis_hash(), 2);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);

    const auto purged = organize(make(genesis_hash(), 2), false);
    BOOST_REQUIRE_EQUAL(purged.first, system::error::checkpoint_conflict);
    BOOST_REQUIRE_EQUAL(purged.second, 0u);

    const auto orphan = organize(make(system::one_hash, 4), false);
    BOOST_REQUIRE_EQUAL(orphan.first, error::orphan_header);
}

////BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__above_reached_checkpoint__reorganized, chaser_header_checkpoint_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto a2 = make(a1->hash(), 3);
////    const auto a3 = make(a2->hash(), 4);
////    const auto c3 = make(a2->hash(), 5);
////    const auto c4 = make(c3->hash(), 6);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(a3, false).first, error::success);

////    const auto weak = organize(c3, false);
////    BOOST_REQUIRE_EQUAL(weak.first, error::success);
////    BOOST_REQUIRE_EQUAL(weak.second, 3u);
////    BOOST_REQUIRE(candidate(3, a3->hash()));

////    const auto result = organize(c4, false);
////    BOOST_REQUIRE_EQUAL(result.first, error::success);
////    BOOST_REQUIRE_EQUAL(result.second, 4u);
////    BOOST_REQUIRE(candidate(3, c3->hash()));
////    BOOST_REQUIRE(candidate(4, c4->hash()));
////}

// window
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__branch_under_window__purged, chaser_header_window_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto a4 = make(a3->hash(), 4);
    const auto b1 = make(genesis_hash(), 5);
    const auto b2 = make(b1->hash(), 6);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a3, false).first, error::success);

    const auto retained = organize(make(b1->hash(), 6), false);
    BOOST_REQUIRE_EQUAL(retained.first, error::duplicate_header);
    BOOST_REQUIRE_EQUAL(retained.second, 2u);

    BOOST_REQUIRE_EQUAL(organize(a4, false).first, error::success);

    const auto purged = organize(make(genesis_hash(), 5), false);
    BOOST_REQUIRE_EQUAL(purged.first, error::success);
    BOOST_REQUIRE_EQUAL(purged.second, 1u);
    BOOST_REQUIRE(candidate(4, a4->hash()));
}

BOOST_FIXTURE_TEST_CASE(chaser_header__organize_proven__sibling_branches_under_window__purged, chaser_header_window_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto a4 = make(a3->hash(), 4);
    const auto b1 = make(genesis_hash(), 5);
    const auto b2 = make(b1->hash(), 6);
    const auto c2 = make(b1->hash(), 7);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(c2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a3, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a4, false).first, error::success);
    BOOST_REQUIRE(candidate(4, a4->hash()));

    const auto purged = organize(make(genesis_hash(), 5), false);
    BOOST_REQUIRE_EQUAL(purged.first, error::success);
    BOOST_REQUIRE_EQUAL(purged.second, 1u);
}

// prioritize
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(chaser_header__prioritize__unknown__not_found, chaser_header_setup_fixture)
{
    const auto result = prioritize(system::one_hash);
    BOOST_REQUIRE_EQUAL(result.first, database::error::not_found);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
}

////BOOST_FIXTURE_TEST_CASE(chaser_header__prioritize__tied_branch__reorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto b1 = make(genesis_hash(), 2);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
////    BOOST_REQUIRE(candidate(1, a1->hash()));

////    const auto result = prioritize(b1->hash());
////    BOOST_REQUIRE_EQUAL(result.first, error::success);
////    BOOST_REQUIRE_EQUAL(result.second, 1u);
////    BOOST_REQUIRE(candidate(1, b1->hash()));
////}

BOOST_FIXTURE_TEST_CASE(chaser_header__prioritize__cached_with_child__unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b1 = make(genesis_hash(), 3);
    const auto b2 = make(b1->hash(), 4);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BOOST_REQUIRE_EQUAL(organize(b2, false).first, error::success);

    const auto result = prioritize(b1->hash());
    BOOST_REQUIRE_EQUAL(result.first, error::success);
    BOOST_REQUIRE_EQUAL(result.second, 0u);
    BOOST_REQUIRE(candidate(1, a1->hash()));
    BOOST_REQUIRE(candidate(2, a2->hash()));
}

// disorganize
// ----------------------------------------------------------------------------

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_top__disorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto a2 = make(a1->hash(), 2);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
////    BOOST_REQUIRE(notify(chases::unvalid{ link(a2->hash()) }, chase::disorganized));
////    flush();

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
////    BOOST_REQUIRE(unconfirmable(a2->hash()));
////    BOOST_REQUIRE(!unconfirmable(a1->hash()));

////    const auto cached = organize(make(genesis_hash(), 1), false);
////    BOOST_REQUIRE_EQUAL(cached.first, error::duplicate_header);
////    BOOST_REQUIRE_EQUAL(cached.second, 1u);
////}

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unchecked_first__disorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto a2 = make(a1->hash(), 2);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
////    BOOST_REQUIRE(notify(chases::unchecked{ link(a1->hash()) }, chase::disorganized));

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
////    BOOST_REQUIRE(unconfirmable(a1->hash()));
////    BOOST_REQUIRE(unconfirmable(a2->hash()));
////}

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unconfirmable_top__disorganized, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE(notify(chases::unconfirmable{ link(a1->hash()) }, chase::disorganized));

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
////    BOOST_REQUIRE(unconfirmable(a1->hash()));
////}

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_above_confirmed__confirmed_candidates, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    const auto a2 = make(a1->hash(), 2);
////    const auto b1 = make(genesis_hash(), 3);
////    const auto b2 = make(b1->hash(), 4);
////    const auto b3 = make(b2->hash(), 5);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(a2, false).first, error::success);
////    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(a1->hash()), false));
////    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(a2->hash()), false));
////    BOOST_REQUIRE_EQUAL(organize(b1, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b2, false).first, error::success);
////    BOOST_REQUIRE_EQUAL(organize(b3, false).first, error::success);
////    BOOST_REQUIRE(candidate(3, b3->hash()));
////    BOOST_REQUIRE(notify(chases::unvalid{ link(b2->hash()) }, chase::disorganized));
////    flush();

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
////    BOOST_REQUIRE(candidate(1, a1->hash()));
////    BOOST_REQUIRE(candidate(2, a2->hash()));
////    BOOST_REQUIRE(unconfirmable(b2->hash()));
////    BOOST_REQUIRE(unconfirmable(b3->hash()));
////    BOOST_REQUIRE(!unconfirmable(b1->hash()));

////    const auto cached = organize(make(genesis_hash(), 3), false);
////    BOOST_REQUIRE_EQUAL(cached.first, error::duplicate_header);
////    BOOST_REQUIRE_EQUAL(cached.second, 1u);
////}

BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_not_candidate__unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto x1 = make(genesis_hash(), 2);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE(archive(*x1, 1, 2));
    BOOST_REQUIRE(notify(chases::unvalid{ link(x1->hash()) }, chase::unvalid));
    flush();

    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BOOST_REQUIRE(candidate(1, a1->hash()));
    BOOST_REQUIRE(!unconfirmable(x1->hash()));
}

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_fork_point__unchanged, chaser_header_setup_fixture)
////{
////    const auto a1 = make(genesis_hash(), 1);
////    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
////    BOOST_REQUIRE(notify(chases::unvalid{ link(genesis_hash()) }, chase::unvalid));
////    flush();

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
////    BOOST_REQUIRE(candidate(1, a1->hash()));
////    BOOST_REQUIRE(!unconfirmable(genesis_hash()));
////}

BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__stop__unsubscribed, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BOOST_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BOOST_REQUIRE(notify(chases::stop{}, chase::stop));
    BOOST_REQUIRE(notify(chases::unvalid{ link(a1->hash()) }, chase::unvalid));
    flush();

    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BOOST_REQUIRE(!unconfirmable(a1->hash()));
}

////BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unstored_fork_point__unchanged, chaser_header_setup_fixture)
////{
////    const auto x2 = make(genesis_hash(), 1);
////    BOOST_REQUIRE(query_.push_candidate(database::header_link{ 42 }));
////    BOOST_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
////    BOOST_REQUIRE(push(*x2, 2, 2));
////    BOOST_REQUIRE(notify(chases::unvalid{ link(x2->hash()) }, chase::unvalid));
////    flush();

////    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
////    BOOST_REQUIRE(!unconfirmable(x2->hash()));
////}

BOOST_FIXTURE_TEST_CASE(chaser_header__handle_chase__unstored_candidate_below__unchanged, chaser_header_setup_fixture)
{
    const auto x2 = make(genesis_hash(), 1);
    BOOST_REQUIRE(query_.push_candidate(database::header_link{ 42 }));
    BOOST_REQUIRE(push(*x2, 2, 2));
    BOOST_REQUIRE(notify(chases::unvalid{ link(x2->hash()) }, chase::unvalid));
    flush();

    BOOST_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BOOST_REQUIRE(!unconfirmable(x2->hash()));
}

BOOST_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
