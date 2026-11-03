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
        return query.set(block1, context1, work1, false, true) &&
            query.push_candidate(query.to_header(block1.hash())) &&
            query.push_confirmed(query.to_header(block1.hash()), true);
    }

    // Mainnet block 1 is a candidate, so the store is not coalesced.
    static bool candidate1(node::query& query) NOEXCEPT
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& block1 = p2p_compact_setup_fixture::block1();
        const database::context context1{ 0, 1, genesis.timestamp() };
        const auto work1 = genesis.proof() + block1.header().proof();
        return query.set(block1, context1, work1, false, true) &&
            query.push_candidate(query.to_header(block1.hash()));
    }

    inline chaser_estimate_setup_fixture(uint16_t horizon,
        const initializer& setup=confirm1)
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
        while (value.first == node::error::estimate_premature &&
            steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(milliseconds(10));
            value = estimate(target, mode);
        }

        return value;
    }

    void notify_pruned(const hash_digest& hash)
    {
        const auto link = query_.to_header(hash);
        node_.notify(node::error::success, chases::pruned{ link.value });
    }

    bool confirm2()
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& block1 = p2p_compact_setup_fixture::block1();
        const auto& block2 = p2p_compact_setup_fixture::block2();
        const database::context context2{ 0, 2, block1.header().timestamp() };
        const auto work1 = genesis.proof() + block1.header().proof();
        const auto work2 = work1 + block2.header().proof();
        return query_.set(block2, context2, work2, false, true) &&
            query_.push_candidate(query_.to_header(block2.hash())) &&
            query_.push_confirmed(query_.to_header(block2.hash()), true);
    }

    bool store_header2()
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& header1 = p2p_compact_setup_fixture::block1().header();
        const auto& header2 = p2p_compact_setup_fixture::block2().header();
        const database::context context2{ 0, 2, header1.timestamp() };
        const auto work2 = genesis.proof() + header1.proof() + header2.proof();
        return query_.set(header2, context2, work2, false);
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

struct chaser_estimate_uncoalesced_setup_fixture
  : chaser_estimate_setup_fixture
{
    inline chaser_estimate_uncoalesced_setup_fixture()
      : chaser_estimate_setup_fixture(2, candidate1)
    {
    }
};

// Runs the protected handlers on the chaser strand.
class estimate_accessor
  : public node::chaser_estimate
{
public:
    using chaser_estimate::chaser_estimate;

    template <typename Method>
    void run(Method&& method) NOEXCEPT
    {
        std::promise<void> promise{};
        network::io::post(strand(), [&]() NOEXCEPT
        {
            method();
            promise.set_value();
        });

        promise.get_future().wait();
    }

    bool chase(const node::event_value& value) NOEXCEPT
    {
        const auto result = handle_chase({}, value);
        run([]() NOEXCEPT {});
        return result;
    }

    void pruned(const database::header_link& link) NOEXCEPT
    {
        run([&]() NOEXCEPT { do_pruned(link.value); });
    }

    void organized(const database::header_link& link) NOEXCEPT
    {
        run([&]() NOEXCEPT { do_organized(link.value); });
    }

    void reorganized(const database::header_link& link) NOEXCEPT
    {
        run([&]() NOEXCEPT { do_reorganized(link.value); });
    }

    void transaction(const database::tx_link& link) NOEXCEPT
    {
        run([&]() NOEXCEPT { do_transaction(link.value); });
    }
};

BC_POP_WARNING()

BC_AUTO_TEST_SUITE(chaser_estimate_tests)

BC_FIXTURE_TEST_CASE(chaser_estimate__estimate__disabled__estimate_disabled, chaser_estimate_disabled_setup_fixture)
{
    const auto result = estimate(1, estimator::mode::basic);
    BC_REQUIRE_EQUAL(result.first, node::error::estimate_disabled);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__estimate__not_initialized__estimate_premature, chaser_estimate_uncoalesced_setup_fixture)
{
    const auto result = estimate(1, estimator::mode::basic);
    BC_REQUIRE_EQUAL(result.first, node::error::estimate_premature);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__estimate__horizon_exceeds_chain__estimate_premature, chaser_estimate_excess_setup_fixture)
{
    notify_pruned(p2p_compact_setup_fixture::block1().hash());
    const auto result = estimate(1, estimator::mode::basic);
    BC_REQUIRE_EQUAL(result.first, node::error::estimate_premature);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__estimate__initialized_no_fees__estimate_false, chaser_estimate_enabled_setup_fixture)
{
    notify_pruned(p2p_compact_setup_fixture::block1().hash());
    const auto result = initialized(1, estimator::mode::basic);
    BC_REQUIRE_EQUAL(result.first, node::error::estimate_false);
    BC_REQUIRE_EQUAL(result.second, estimator::estimate_failed);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__top_height__not_initialized__zero, chaser_estimate_enabled_setup_fixture)
{
    const node::chaser_estimate instance{ node_ };
    BC_REQUIRE(!instance.initialized());
    BC_REQUIRE_EQUAL(instance.top_height(), zero);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__pruned__horizon_within_chain__initialized, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    BC_REQUIRE(instance.initialized());
    BC_REQUIRE_EQUAL(instance.top_height(), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_estimate__organized__next_confirmed__pushed, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    BC_REQUIRE(confirm2());

    const auto link2 = query_.to_header(p2p_compact_setup_fixture::block2().hash());
    instance.organized(link2);
    BC_REQUIRE_EQUAL(instance.top_height(), 2u);

    instance.organized(link2);
    BC_REQUIRE_EQUAL(instance.top_height(), 2u);
    BC_REQUIRE(!node_.suspended());
}

BC_FIXTURE_TEST_CASE(chaser_estimate__reorganized__top__popped, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    BC_REQUIRE(confirm2());

    const auto link2 = query_.to_header(p2p_compact_setup_fixture::block2().hash());
    instance.organized(link2);
    instance.reorganized(link2);
    BC_REQUIRE_EQUAL(instance.top_height(), 1u);

    instance.reorganized(link2);
    BC_REQUIRE_EQUAL(instance.top_height(), 1u);
    BC_REQUIRE(!node_.suspended());
}

BC_FIXTURE_TEST_CASE(chaser_estimate__handle_chase__initialized__handled, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    const auto link1 = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    BC_REQUIRE(instance.chase(chases::block{ link1.value }));
    BC_REQUIRE(!instance.initialized());

    BC_REQUIRE(instance.chase(chases::pruned{ link1.value }));
    BC_REQUIRE(instance.initialized());
    BC_REQUIRE(confirm2());

    const auto link2 = query_.to_header(p2p_compact_setup_fixture::block2().hash());
    BC_REQUIRE(instance.chase(chases::organized{ link2.value }));
    BC_REQUIRE_EQUAL(instance.top_height(), 2u);

    BC_REQUIRE(instance.chase(chases::reorganized{ link2.value }));
    BC_REQUIRE_EQUAL(instance.top_height(), 1u);

    const auto coinbase = p2p_compact_setup_fixture::block2().transactions_ptr()->front();
    BC_REQUIRE(instance.chase(chases::transaction{ query_.to_tx(coinbase->hash(false)).value }));
    BC_REQUIRE(instance.chase(chases::bump{}));
    BC_REQUIRE(!instance.chase(chases::stop{}));
}

BC_FIXTURE_TEST_CASE(chaser_estimate__organized__unstored_link__suspended, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    instance.organized(database::header_link{ 42 });
    BC_REQUIRE(suspended());
}

BC_FIXTURE_TEST_CASE(chaser_estimate__organized__header_only__suspended, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    BC_REQUIRE(store_header2());
    instance.organized(query_.to_header(p2p_compact_setup_fixture::block2().hash()));
    BC_REQUIRE(suspended());
}

BC_FIXTURE_TEST_CASE(chaser_estimate__reorganized__unstored_link__suspended, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    instance.reorganized(database::header_link{ 42 });
    BC_REQUIRE(suspended());
}

BC_FIXTURE_TEST_CASE(chaser_estimate__transaction__unstored_link__suspended, chaser_estimate_enabled_setup_fixture)
{
    estimate_accessor instance{ node_ };
    instance.pruned(query_.to_header(p2p_compact_setup_fixture::block1().hash()));
    instance.transaction(database::tx_link{ 42 });
    BC_REQUIRE(suspended());
}

BC_AUTO_TEST_SUITE_END()
