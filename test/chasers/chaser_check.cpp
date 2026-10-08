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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

using namespace system;
using namespace network::messages::peer;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A current node whose chaser events are observable by the test.
struct chaser_check_setup_fixture
  : p2p_setup_fixture
{
    struct work
    {
        code ec;
        map_ptr map;
        job::ptr racer;
    };

    inline chaser_check_setup_fixture(const initializer& setup={},
        const configurator& configure={})
      : p2p_setup_fixture(setup, [=](configuration& config)
        {
            config.node.currency_window_minutes = 0;
            config.bitcoin.minimum_work = {};
            if (configure)
                configure(config);
        })
    {
    }

    /// Wait (bounded) for the block to be associated in the store.
    bool associated(const hash_digest& hash)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (query_.is_associated(query_.to_header(hash)))
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    /// Report channel performance to the check chaser.
    code update(object_key key, uint64_t speed)
    {
        std::promise<code> promise{};
        node_.performance(key, speed, [&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        });

        return promise.get_future().get();
    }

    /// Obtain work from the check chaser.
    work get_hashes()
    {
        std::promise<work> promise{};
        node_.get_hashes([&](const code& ec, const map_ptr& map,
            const job::ptr& job) NOEXCEPT
        {
            promise.set_value({ ec, map, job });
        });

        return promise.get_future().get();
    }

    /// Return work to the check chaser.
    code put_hashes(const map_ptr& map)
    {
        std::promise<code> promise{};
        node_.put_hashes(map, [&](const code& ec) NOEXCEPT
        {
            promise.set_value(ec);
        });

        return promise.get_future().get();
    }

    /// Take all candidate work from the check chaser as one map.
    map_ptr take_candidates()
    {
        const auto map = get_hashes().map;
        map->merge(*get_hashes().map);
        return map;
    }
};

// Mainnet blocks 1 and 2 are unassociated candidates.
struct chaser_check_candidate_setup_fixture
  : chaser_check_setup_fixture
{
    inline chaser_check_candidate_setup_fixture(
        const configurator& configure={})
      : chaser_check_setup_fixture(
            p2p_compact_candidate_setup_fixture::candidate,
            [=](configuration& config)
            {
                config.node.sample_period_seconds = 0;
                if (configure)
                    configure(config);
            })
    {
    }
};

// Candidates on a node that does not store the blocks it has pruned.
struct chaser_check_pruned_setup_fixture
  : chaser_check_candidate_setup_fixture
{
    inline chaser_check_pruned_setup_fixture()
      : chaser_check_candidate_setup_fixture([](configuration& config)
        {
            config.node.limited_blocks = true;
        })
    {
    }
};

// Candidates with a one second performance sample period.
struct chaser_check_sampled_setup_fixture
  : chaser_check_candidate_setup_fixture
{
    inline chaser_check_sampled_setup_fixture()
      : chaser_check_candidate_setup_fixture([](configuration& config)
        {
            config.node.sample_period_seconds = 1;
        })
    {
    }
};

// Candidates with a one second sample period and no deviation monitor.
struct chaser_check_sampled_local_setup_fixture
  : chaser_check_candidate_setup_fixture
{
    inline chaser_check_sampled_local_setup_fixture()
      : chaser_check_candidate_setup_fixture([](configuration& config)
        {
            config.node.sample_period_seconds = 1;
            config.node.allowed_deviation = 0;
        })
    {
    }
};

// Mainnet block 1 is an unassociated compact candidate.
struct chaser_check_compact_setup_fixture
  : chaser_check_setup_fixture
{
    static bool compact(node::query& query) NOEXCEPT
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const auto& header1 = p2p_compact_setup_fixture::block1().header();
        const database::context context1{ 0, 1, genesis.timestamp() };
        database::header_link link{};
        const auto work = genesis.proof() + header1.proof();
        return !query.set_code(link, header1, context1, work, false, true) &&
            query.push_candidate(link);
    }

    inline chaser_check_compact_setup_fixture()
      : chaser_check_setup_fixture(compact, [](configuration& config)
        {
            config.node.compact_timeout_seconds = 1;
        })
    {
    }
};

// A compact candidate deferred beyond a one second performance sample period.
struct chaser_check_compact_sampled_setup_fixture
  : chaser_check_setup_fixture
{
    inline chaser_check_compact_sampled_setup_fixture()
      : chaser_check_setup_fixture(
            chaser_check_compact_setup_fixture::compact,
            [](configuration& config)
            {
                config.node.compact_timeout_seconds = 3;
                config.node.sample_period_seconds = 1;
            })
    {
    }
};

BC_POP_WARNING()

namespace peer = network::messages::peer;

static constexpr uint64_t peer_services = service::node_network |
    service::node_witness;

// Mainnet block 1 with a coinbase witness, which does not change its hash.
static const chain::block& witness_block1() NOEXCEPT
{
    const auto data = base16_chunk
    (
        "010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d61900"
        "00000000982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e8"
        "57233e0e61bc6649ffff001d01e3629901010000000001010000000000000000"
        "000000000000000000000000000000000000000000000000ffffffff0704ffff"
        "001d0104ffffffff0100f2052a0100000043410496b538e853519c726a2c91e6"
        "1ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f814"
        "1781e62294721166bf621e73a82cbf2342c858eeac0120000000000000000000"
        "000000000000000000000000000000000000000000000000000000"
    );
    static const chain::block instance{ data, true };

    return instance;
}

static database::association item(size_t height) NOEXCEPT
{
    chain::context context{};
    context.height = height;
    return { {}, { possible_narrow_cast<uint8_t>(height) }, context };
}

BOOST_AUTO_TEST_SUITE(chaser_check_tests)

// split

BOOST_AUTO_TEST_CASE(chaser_check__split__four__lower_half_split)
{
    const auto map = chaser_check::empty_map();
    map->insert(item(4));
    map->insert(item(1));
    map->insert(item(3));
    map->insert(item(2));

    const auto half = chaser_check::split(map);
    BOOST_REQUIRE_EQUAL(half->size(), two);
    BOOST_REQUIRE_EQUAL(map->size(), two);
    BOOST_REQUIRE(half->exists(size_t{ 1 }));
    BOOST_REQUIRE(half->exists(size_t{ 2 }));
    BOOST_REQUIRE(map->exists(size_t{ 3 }));
    BOOST_REQUIRE(map->exists(size_t{ 4 }));
}

BOOST_AUTO_TEST_CASE(chaser_check__split__one__empty_split)
{
    const auto map = chaser_check::empty_map();
    map->insert(item(1));

    const auto half = chaser_check::split(map);
    BOOST_REQUIRE(half->empty());
    BOOST_REQUIRE_EQUAL(map->size(), one);
}

// update

BOOST_FIXTURE_TEST_CASE(chaser_check__update__maximum__exhausted_channel, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(update(1, max_uint64) == node::error::exhausted_channel);
}

BOOST_FIXTURE_TEST_CASE(chaser_check__update__zero__stalled_channel, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(update(1, 0) == node::error::stalled_channel);
}

BOOST_FIXTURE_TEST_CASE(chaser_check__update__below_minimum_count__success, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(!update(1, 100));
    BOOST_REQUIRE(!update(2, 1));
    BOOST_REQUIRE(!update(3, 1));
}

BOOST_FIXTURE_TEST_CASE(chaser_check__update__not_below_mean__success, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(!update(1, 100));
    BOOST_REQUIRE(!update(2, 100));
    BOOST_REQUIRE(!update(3, 100));
    BOOST_REQUIRE(!update(4, 200));
}

BOOST_FIXTURE_TEST_CASE(chaser_check__update__below_mean_within_deviation__success, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(!update(1, 100));
    BOOST_REQUIRE(!update(2, 200));
    BOOST_REQUIRE(!update(3, 300));
    BOOST_REQUIRE(!update(4, 150));
}

BOOST_FIXTURE_TEST_CASE(chaser_check__update__below_mean_beyond_deviation__slow_channel, chaser_check_setup_fixture)
{
    BOOST_REQUIRE(!update(1, 100));
    BOOST_REQUIRE(!update(2, 100));
    BOOST_REQUIRE(!update(3, 100));
    BOOST_REQUIRE(!update(4, 100));
    BOOST_REQUIRE(update(5, 10) == node::error::slow_channel);
}

// starved

BOOST_FIXTURE_TEST_CASE(chaser_check__starved__recorded_speeds__slowest_other_split, chaser_check_setup_fixture)
{
    const auto key = subscribe();
    BOOST_REQUIRE(!update(key, 100));
    BOOST_REQUIRE(!update(42, 1));
    node_.notify({}, chases::starved{ 42 });

    chases::split split{};
    BOOST_REQUIRE(await(split));
    BOOST_REQUIRE_EQUAL(split.channel, 42u);
}

BOOST_FIXTURE_TEST_CASE(chaser_check__starved__no_speeds__stall, chaser_check_setup_fixture)
{
    subscribe();
    node_.notify({}, chases::starved{ 42 });

    chases::stall stall{};
    BOOST_REQUIRE(await(stall));
    BOOST_REQUIRE_EQUAL(stall.channel, 42u);
}

// regressed

////BOOST_FIXTURE_TEST_CASE(chaser_check__regressed__below_window__purged_and_downloaded, chaser_check_candidate_setup_fixture)
////{
////    subscribe();
////    node_.notify({}, chases::regressed{ 0 });

////    chases::purge purge{};
////    BOOST_REQUIRE(await(purge));
////    BOOST_REQUIRE_EQUAL(purge.branch_point, zero);

////    chases::download download{};
////    BOOST_REQUIRE(await(download));
////    BOOST_REQUIRE_EQUAL(download.count, two);
////    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block1().hash()));
////    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block2().hash()));
////}

////BOOST_FIXTURE_TEST_CASE(chaser_check__regressed__above_window__not_purged, chaser_check_candidate_setup_fixture)
////{
////    subscribe();
////    node_.notify({}, chases::regressed{ 5 });
////    node_.notify({}, chases::regressed{ 0 });

////    chases::purge purge{};
////    BOOST_REQUIRE(await(purge));
////    BOOST_REQUIRE_EQUAL(purge.branch_point, zero);
////}

////BOOST_FIXTURE_TEST_CASE(chaser_check__disorganized__below_window__purged, chaser_check_candidate_setup_fixture)
////{
////    subscribe();
////    node_.notify({}, chases::disorganized{ 0 });

////    chases::purge purge{};
////    BOOST_REQUIRE(await(purge));
////    BOOST_REQUIRE_EQUAL(purge.branch_point, zero);
////}

// get_hashes/put_hashes

BOOST_FIXTURE_TEST_CASE(chaser_check__get_hashes__candidates__expected_map, chaser_check_candidate_setup_fixture)
{
    const auto work = get_hashes();
    BOOST_REQUIRE(!work.ec);
    BOOST_REQUIRE(work.racer);
    BOOST_REQUIRE_EQUAL(work.map->size(), one);
    BOOST_REQUIRE(work.map->exists(p2p_compact_setup_fixture::block1().hash()));
    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block2().hash()));
    BOOST_REQUIRE(get_hashes().map->empty());
}

BOOST_FIXTURE_TEST_CASE(chaser_check__put_hashes__taken_map__download_and_restored, chaser_check_candidate_setup_fixture)
{
    subscribe();
    const auto map = take_candidates();
    BOOST_REQUIRE(!put_hashes(map));

    chases::download download{};
    BOOST_REQUIRE(await(download));
    BOOST_REQUIRE_EQUAL(download.count, two);
    BOOST_REQUIRE_EQUAL(get_hashes().map->size(), two);
}

BOOST_FIXTURE_TEST_CASE(chaser_check__get_hashes__purging__dropped, chaser_check_candidate_setup_fixture)
{
    subscribe();
    auto work = get_hashes();
    node_.notify({}, chases::regressed{ 0 });

    chases::purge purge{};
    BOOST_REQUIRE(await(purge));

    std::atomic_bool got{ false };
    std::atomic_bool put{ false };
    node_.get_hashes([&](const code&, const map_ptr&, const job::ptr&) NOEXCEPT { got = true; });
    node_.put_hashes(work.map, [&](const code&) NOEXCEPT { put = true; });
    work.racer.reset();

    chases::download download{};
    BOOST_REQUIRE(await(download));
    BOOST_REQUIRE_EQUAL(download.count, two);
    BOOST_REQUIRE(!got);
    BOOST_REQUIRE(!put);
}

// checked

BOOST_FIXTURE_TEST_CASE(chaser_check__checked__window_downloaded__windowed, chaser_check_candidate_setup_fixture)
{
    subscribe();
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);
    send(peer::block::command, p2p_compact_setup_fixture::block1().to_data(true));
    receive(get_data::command);
    send(peer::block::command, p2p_compact_setup_fixture::block2().to_data(true));

    chases::windowed windowed{};
    BOOST_REQUIRE(await(windowed));
    BOOST_REQUIRE_EQUAL(windowed.height, two);
}

// valid

BOOST_FIXTURE_TEST_CASE(chaser_check__valid__requested_not_positioned__no_download, chaser_check_candidate_setup_fixture)
{
    subscribe();
    node_.notify({}, chases::valid{ 1 });
    node_.notify({}, chases::valid{ 2 });
    node_.notify({}, chases::starved{ 42 });

    chases::stall stall{};
    BOOST_REQUIRE(await(stall));
    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block1().hash()));
    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block2().hash()));
    BOOST_REQUIRE(get_hashes().map->empty());
}

// bump

BOOST_FIXTURE_TEST_CASE(chaser_check__bump__purging__download_on_purged, chaser_check_candidate_setup_fixture)
{
    subscribe();
    auto work = get_hashes();
    node_.notify({}, chases::regressed{ 0 });

    chases::purge purge{};
    BOOST_REQUIRE(await(purge));

    node_.notify({}, chases::bump{ 0 });
    node_.notify({}, chases::starved{ 42 });

    chases::stall stall{};
    BOOST_REQUIRE(await(stall));
    work.racer.reset();

    chases::download download{};
    BOOST_REQUIRE(await(download));
    BOOST_REQUIRE_EQUAL(download.count, two);
}

// compact

////BOOST_FIXTURE_TEST_CASE(chaser_check__handle_compact_timer__expired__download, chaser_check_compact_setup_fixture)
////{
////    subscribe();

////    chases::download download{};
////    BOOST_REQUIRE(await(download));
////    BOOST_REQUIRE_EQUAL(download.count, one);
////    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block1().hash()));
////}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(protocol_block_in_31800_tests)

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__subscribed__candidates__get_data, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));

    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE_EQUAL(request->items.size(), one);
    BOOST_REQUIRE(request->items.front().hash == p2p_compact_setup_fixture::block1().hash());
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_receive_block__unrequested__not_stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(peer::block::command, genesis.to_data(true));
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_receive_block__malleated_transactions__stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    const auto& block1 = p2p_compact_setup_fixture::block1();
    const chain::block malleated{ block1.header(), { *p2p_compact_setup_fixture::block2().transactions_ptr()->front() } };
    BOOST_REQUIRE(malleated.hash() == block1.hash());
    send(peer::block::command, malleated.to_data(true));

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
    BOOST_REQUIRE(!query_.is_associated(query_.to_header(block1.hash())));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_receive_block__witness_before_bip141__stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    BOOST_REQUIRE(witness_block1().hash() == p2p_compact_setup_fixture::block1().hash());
    send(peer::block::command, witness_block1().to_data(true));

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
    BOOST_REQUIRE(!query_.is_associated(query_.to_header(witness_block1().hash())));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_receive_block__pruned_witness__stopped, chaser_check_pruned_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    send(peer::block::command, witness_block1().to_data(true));

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
    BOOST_REQUIRE(!query_.is_associated(query_.to_header(witness_block1().hash())));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_receive_block__pruned__associated, chaser_check_pruned_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    const auto& block1 = p2p_compact_setup_fixture::block1();
    send(peer::block::command, block1.to_data(true));

    BOOST_REQUIRE(associated(block1.hash()));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__report__not_stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);
    subscribe();

    node_.notify({}, chases::report{ 1 });
    chases::report report{};
    BOOST_REQUIRE(await(report));
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__purge_with_work__stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    node_.notify({}, chases::purge{ 0 });

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__stall_with_work__stopped_and_work_divided, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(!put_hashes(take_candidates()));
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    node_.notify({}, chases::stall{ 42 });
    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);

    const auto map = get_hashes().map;
    BOOST_REQUIRE_EQUAL(map->size(), one);
    BOOST_REQUIRE(map->exists(p2p_compact_setup_fixture::block1().hash()));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__download_then_split__stopped, chaser_check_candidate_setup_fixture)
{
    subscribe();
    const auto map = take_candidates();
    BOOST_REQUIRE(handshake(peer_services));

    chases::starved starved{};
    BOOST_REQUIRE(await(starved));
    BOOST_REQUIRE(!put_hashes(map));

    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE_EQUAL(request->items.size(), two);

    node_.notify_one(starved.channel, {}, chases::split{ 42 });
    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block1().hash()));
    BOOST_REQUIRE(get_hashes().map->exists(p2p_compact_setup_fixture::block2().hash()));
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__download_with_work__not_stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);
    subscribe();

    node_.notify({}, chases::download{ 1 });
    chases::download download{};
    BOOST_REQUIRE(await(download));
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__purge_without_work__not_stopped, chaser_check_setup_fixture)
{
    subscribe();
    BOOST_REQUIRE(handshake(peer_services));

    chases::starved starved{};
    BOOST_REQUIRE(await(starved));

    node_.notify({}, chases::purge{ 0 });
    node_.notify({}, chases::download{ 1 });
    BOOST_REQUIRE(await(starved));
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__handle_chase__split_single__not_stopped, chaser_check_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);
    subscribe();

    node_.notify({}, chases::split{ 42 });
    chases::split split{};
    BOOST_REQUIRE(await(split));
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

// performance

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__performance__no_bytes__stopped, chaser_check_sampled_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__performance__no_bytes_no_deviation__stopped, chaser_check_sampled_local_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));
    receive(get_data::command);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

////BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__performance__partial_then_no_bytes__stopped, chaser_check_sampled_setup_fixture)
////{
////    BOOST_REQUIRE(handshake(peer_services));
////    receive(get_data::command);

////    const auto& block1 = p2p_compact_setup_fixture::block1();
////    send(peer::block::command, block1.to_data(true));
////    BOOST_REQUIRE(associated(block1.hash()));

////    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
////}

BOOST_FIXTURE_TEST_CASE(protocol_block_in_31800__performance__idle__deferred_get_data, chaser_check_compact_sampled_setup_fixture)
{
    BOOST_REQUIRE(handshake(peer_services));

    const auto request = get_data::deserialize(node_version->value, receive(get_data::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE_EQUAL(request->items.size(), one);
    BOOST_REQUIRE(request->items.front().hash == p2p_compact_setup_fixture::block1().hash());
}

BOOST_AUTO_TEST_SUITE_END()
