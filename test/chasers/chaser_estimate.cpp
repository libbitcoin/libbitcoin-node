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
#include <chrono>
#include <future>
#include <thread>

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

struct chaser_estimate_setup_fixture
  : p2p_setup_fixture
{
    using result = std::pair<code, uint64_t>;

    // Mainnet block 1 is confirmed.
    static bool confirm1(node::query& query) NOEXCEPT
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& block1 = p2p_compact_setup_fixture::block1();
        const database::context context1{ 0, 1, genesis.timestamp() };
        const auto work1 = genesis.proof() + block1.header().proof();
        return query.set(block1, context1, work1, false, true) && query.push_candidate(query.to_header(block1.hash())) && query.push_confirmed(query.to_header(block1.hash()), true);
    }

    inline chaser_estimate_setup_fixture(uint16_t horizon, const initializer& setup=confirm1)
      : p2p_setup_fixture(setup, [=](configuration& config)
        {
            config.node.fee_estimate_horizon = horizon;
        })
    {
    }

    result estimate(size_t target, estimator::mode mode)
    {
        std::promise<result> promise{};
        node_.estimate(target, mode, [&](const code& ec, uint64_t value) NOEXCEPT
        {
            promise.set_value({ ec, value });
        });

        return promise.get_future().get();
    }

    // Wait (bounded) for the estimator to leave the premature state.
    result initialized(size_t target, estimator::mode mode)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        auto value = estimate(target, mode);
        while (value.first == node::error::estimate_premature && steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(milliseconds(10));
            value = estimate(target, mode);
        }

        return value;
    }

    void notify_block(const hash_digest& hash)
    {
        node_.notify(node::error::success, chases::block{ query_.to_header(hash).value });
    }

    void notify_organized(const hash_digest& hash)
    {
        node_.notify(node::error::success, chases::organized{ query_.to_header(hash).value });
    }

    void notify_reorganized(const hash_digest& hash)
    {
        node_.notify(node::error::success, chases::reorganized{ query_.to_header(hash).value });
    }

    bool confirm2()
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& block1 = p2p_compact_setup_fixture::block1();
        const auto& block2 = p2p_compact_setup_fixture::block2();
        const database::context context2{ 0, 2, block1.header().timestamp() };
        const auto work2 = genesis.proof() + block1.header().proof() + block2.header().proof();
        return query_.set(block2, context2, work2, false, true) && query_.push_candidate(query_.to_header(block2.hash())) && query_.push_confirmed(query_.to_header(block2.hash()), true);
    }

    bool store_header2()
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& header1 = p2p_compact_setup_fixture::block1().header();
        const auto& header2 = p2p_compact_setup_fixture::block2().header();
        const database::context context2{ 0, 2, header1.timestamp() };
        return query_.set(header2, context2, genesis.proof() + header1.proof() + header2.proof(), false);
    }

    bool suspended()
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (node_.suspended())
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }
};

struct chaser_estimate_disabled_setup_fixture
  : chaser_estimate_setup_fixture
{
    inline chaser_estimate_disabled_setup_fixture()
      : chaser_estimate_setup_fixture(0)
    {
    }
};

struct chaser_estimate_excess_setup_fixture
  : chaser_estimate_setup_fixture
{
    inline chaser_estimate_excess_setup_fixture()
      : chaser_estimate_setup_fixture(3)
    {
    }
};

struct chaser_estimate_enabled_setup_fixture
  : chaser_estimate_setup_fixture
{
    inline chaser_estimate_enabled_setup_fixture()
      : chaser_estimate_setup_fixture(2)
    {
    }
};

BC_POP_WARNING()

BOOST_AUTO_TEST_SUITE(chaser_estimate_tests)

BOOST_FIXTURE_TEST_CASE(chaser_estimate__estimate__disabled__estimate_disabled, chaser_estimate_disabled_setup_fixture)
{
    const auto result = estimate(1, estimator::mode::basic);
    BOOST_REQUIRE_EQUAL(result.first, node::error::estimate_disabled);
}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__estimate__not_initialized__estimate_premature, chaser_estimate_enabled_setup_fixture)
{
    const auto result = estimate(1, estimator::mode::basic);
    BOOST_REQUIRE_EQUAL(result.first, node::error::estimate_premature);
}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__estimate__horizon_exceeds_chain__estimate_premature, chaser_estimate_excess_setup_fixture)
{
    notify_block(p2p_compact_setup_fixture::block1().hash());
    const auto result = estimate(1, estimator::mode::basic);
    BOOST_REQUIRE_EQUAL(result.first, node::error::estimate_premature);
}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__estimate__initialized_no_fees__estimate_false, chaser_estimate_enabled_setup_fixture)
{
    notify_block(p2p_compact_setup_fixture::block1().hash());
    const auto result = initialized(1, estimator::mode::basic);
    BOOST_REQUIRE_EQUAL(result.first, node::error::estimate_false);
    BOOST_REQUIRE_EQUAL(result.second, estimator::estimate_failed);
}

////BOOST_FIXTURE_TEST_CASE(chaser_estimate__estimate__organized_and_reorganized__estimate_false, chaser_estimate_enabled_setup_fixture)
////{
////    const auto& block1 = p2p_compact_setup_fixture::block1();
////    const auto& block2 = p2p_compact_setup_fixture::block2();
////    notify_block(block1.hash());
////    BOOST_REQUIRE_EQUAL(initialized(1, estimator::mode::basic).first, node::error::estimate_false);

////    notify_block(block1.hash());
////    notify_organized(block1.hash());
////    BOOST_REQUIRE(confirm2());
////    notify_organized(block2.hash());
////    notify_reorganized(block2.hash());
////    notify_reorganized(block2.hash());
////    BOOST_REQUIRE_EQUAL(estimate(1, estimator::mode::basic).first, node::error::estimate_false);
////}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__top_height__not_initialized__zero, chaser_estimate_enabled_setup_fixture)
{
    const node::chaser_estimate instance{ node_ };
    BOOST_REQUIRE(!instance.initialized());
    BOOST_REQUIRE_EQUAL(instance.top_height(), zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__organized__invalid_link__suspended, chaser_estimate_enabled_setup_fixture)
{
    notify_block(p2p_compact_setup_fixture::block1().hash());
    BOOST_REQUIRE_EQUAL(initialized(1, estimator::mode::basic).first, node::error::estimate_false);

    node_.notify(node::error::success, chases::organized{ node::header_t{ 42 } });
    BOOST_REQUIRE(suspended());
}

BOOST_FIXTURE_TEST_CASE(chaser_estimate__organized__unconfirmed_above_top__suspended, chaser_estimate_enabled_setup_fixture)
{
    notify_block(p2p_compact_setup_fixture::block1().hash());
    BOOST_REQUIRE_EQUAL(initialized(1, estimator::mode::basic).first, node::error::estimate_false);

    BOOST_REQUIRE(store_header2());
    notify_organized(p2p_compact_setup_fixture::block2().hash());
    BOOST_REQUIRE(suspended());
}

////BOOST_FIXTURE_TEST_CASE(chaser_estimate__reorganized__invalid_link__suspended, chaser_estimate_enabled_setup_fixture)
////{
////    notify_block(p2p_compact_setup_fixture::block1().hash());
////    BOOST_REQUIRE_EQUAL(initialized(1, estimator::mode::basic).first, node::error::estimate_false);

////    node_.notify(node::error::success, chases::reorganized{ node::header_t{ 42 } });
////    BOOST_REQUIRE(suspended());
////}

BOOST_AUTO_TEST_SUITE_END()
