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
namespace peer = network::messages::peer;
using type_id = peer::inventory_item::type_id;

static constexpr uint64_t parent_value = 100'000;
static const hash_digest missing_hash{ 0x42 };

// An unconfirmed tx with an anyone-can-spend output.
static const chain::transaction& parent() NOEXCEPT
{
    const chain::point point{ one_hash, 0u };
    const chain::operations ops{ { chain::opcode::push_positive_1 } };
    const chain::script spendable{ ops };
    static const chain::transaction instance
    {
        1u,
        chain::inputs{ { point, chain::script{}, max_uint32 } },
        chain::outputs{ { parent_value, spendable } },
        0u
    };

    return instance;
}

static chain::transaction::cptr spend(const hash_digest& hash, uint64_t value,
    uint32_t locktime=0, uint32_t sequence=max_uint32) NOEXCEPT
{
    const chain::point point{ hash, 0u };
    const chain::operations ops{ { chain::opcode::push_positive_1 } };
    const chain::script spendable{ ops };
    return std::make_shared<const chain::transaction>(chain::transaction
    {
        1u,
        chain::inputs{ { point, chain::script{}, sequence } },
        chain::outputs{ { value, spendable } },
        locktime
    });
}

static chain::transaction::cptr spend(uint64_t value) NOEXCEPT
{
    return spend(parent().hash(false), value);
}

static chain::transaction::cptr spend_relative(const hash_digest& hash,
    uint64_t value, uint32_t sequence) NOEXCEPT
{
    const chain::point point{ hash, 0u };
    const chain::operations ops{ { chain::opcode::push_positive_1 } };
    const chain::script spendable{ ops };
    return std::make_shared<const chain::transaction>(chain::transaction
    {
        2u,
        chain::inputs{ { point, chain::script{}, sequence } },
        chain::outputs{ { value, spendable } },
        0u
    });
}

static chain::transactions_cptr package(
    const chain::transaction_cptrs& txs) NOEXCEPT
{
    return std::make_shared<const chain::transaction_cptrs>(txs);
}

static peer::inventory announcement(size_t count, uint8_t tag) NOEXCEPT
{
    peer::inventory message{};
    message.items.reserve(count);
    for (size_t index = 0; index < count; ++index)
    {
        hash_digest hash{};
        hash.at(0) = tag;
        hash.at(1) = static_cast<uint8_t>(index);
        hash.at(2) = static_cast<uint8_t>(index >> 8);
        hash.at(3) = static_cast<uint8_t>(index >> 16);
        message.items.emplace_back(type_id::transaction, hash);
    }

    return message;
}

static bool stored_parent(node::query& query) NOEXCEPT
{
    return query.set(parent());
}

// Mainnet block 1 archived as a header without txs.
static bool stored_header(node::query& query) NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const auto& header1 = p2p_compact_setup_fixture::block1().header();
    const database::context context1{ 0, 1, genesis.timestamp() };
    const auto work1 = genesis.proof() + header1.proof();
    return query.set(parent()) && query.set(header1, context1, work1, false);
}

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

struct chaser_transaction_setup_fixture
  : p2p_setup_fixture
{
    using result = std::pair<code, size_t>;

    inline chaser_transaction_setup_fixture(const configurator& configure={},
        const initializer& setup=stored_parent)
      : p2p_setup_fixture(setup, configure)
    {
    }

    result submit(const chain::transactions_cptr& txs, bool test=false)
    {
        std::promise<result> promise{};
        node_.submit(txs, test, [&](const code& ec, size_t index) NOEXCEPT
        {
            promise.set_value({ ec, index });
        });

        return promise.get_future().get();
    }

    result submit_compact(const chain::transactions_cptr& txs,
        const database::tx_links& links, const database::header_link& link)
    {
        std::promise<result> promise{};
        const auto handler = [&](const code& ec, size_t index) NOEXCEPT
        {
            promise.set_value({ ec, index });
        };

        node_.submit_compact(txs, links, link, handler);

        return promise.get_future().get();
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

    bool archived(const hash_digest& hash)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (query_.is_tx(hash))
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    bool pooled(const database::tx_link& link)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (query_.is_pooled(link))
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    // True if the command is received before the pong of a subsequent ping.
    bool received_before_pong(const std::string& command)
    {
        send(peer::ping{ 42 }, node_version->value);
        while (true)
        {
            const auto message = receive();
            if (message.first == command)
                return true;

            if (message.first == peer::pong::command)
                return false;
        }
    }

    // The payload of the command if received before the pong of a subsequent
    // ping, otherwise empty.
    data_chunk receive_before_pong(const std::string& command)
    {
        send(peer::ping{ 42 }, node_version->value);
        while (true)
        {
            auto message = receive();
            if (message.first == command)
                return std::move(message.second);

            if (message.first == peer::pong::command)
                return {};
        }
    }

    // Wait (bounded) for the command, pinging until it is received.
    data_chunk await_before_pong(const std::string& command)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            auto payload = receive_before_pong(command);
            if (!payload.empty())
                return payload;
        }

        return {};
    }
};

struct chaser_transaction_relay_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_relay_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
        })
    {
    }
};

struct chaser_transaction_pooling_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_pooling_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

struct chaser_transaction_fee_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_fee_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.minimum_fee_rate = 0.0000152587890625;
        })
    {
    }
};

struct chaser_transaction_no_witness_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_no_witness_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.provide_witness = false;
        })
    {
    }
};

struct chaser_transaction_relative_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_relative_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            const auto genesis = config.bitcoin.genesis_block.hash();
            config.bitcoin.bip9_bit0_active_checkpoint = { genesis, 0 };
        })
    {
    }
};

struct chaser_transaction_header_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_header_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        }, stored_header)
    {
    }
};

struct chaser_transaction_memory_pool_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_memory_pool_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.enable_memory_pool = true;
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

struct chaser_transaction_witness_memory_pool_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_witness_memory_pool_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.enable_memory_pool = true;
            config.network.enable_witness_tx = true;
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

BC_POP_WARNING()

BOOST_AUTO_TEST_SUITE(chaser_transaction_tests)

// submit

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__relay_disabled__pooling_disabled, chaser_transaction_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__not_current__pooling_disabled, chaser_transaction_relay_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__empty__empty_package, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({}));
    BOOST_REQUIRE_EQUAL(result.first, node::error::empty_package);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__stored__duplicate_transaction, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ std::make_shared<const chain::transaction>(parent()) }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::duplicate_transaction);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__forward_reference__forward_reference, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto result = submit(package({ second, first }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::forward_reference);
    BOOST_REQUIRE(!query_.is_tx(first->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__internal_double_spend__block_internal_double_spend, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value), spend(sub1(parent_value)) }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::block_internal_double_spend);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__coinbase__coinbase_transaction, chaser_transaction_pooling_setup_fixture)
{
    const chain::block& genesis = config_.bitcoin.genesis_block;
    const auto result = submit(package({ genesis.transactions_ptr()->front() }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::coinbase_transaction);
    BOOST_REQUIRE_EQUAL(result.second, zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__empty_transaction__empty_transaction, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ std::make_shared<const chain::transaction>() }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::empty_transaction);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__missing_prevout__missing_previous_output, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(missing_hash, parent_value) }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::missing_previous_output);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__overspent__spend_exceeds_value, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(add1(parent_value)) }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::spend_exceeds_value);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__second_overspent__index_one, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), add1(parent_value));
    const auto result = submit(package({ first, second }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::spend_exceeds_value);
    BOOST_REQUIRE_EQUAL(result.second, one);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__absolute_locked__absolute_time_locked, chaser_transaction_pooling_setup_fixture)
{
    const auto result = submit(package({ spend(parent().hash(false), parent_value, 1000, 0) }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::absolute_time_locked);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__valid_test__success_not_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const auto result = submit(package({ tx }), true);
    BOOST_REQUIRE_EQUAL(result.first, node::error::success);
    BOOST_REQUIRE(!query_.is_tx(tx->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__valid__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    const auto result = submit(package({ tx }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::success);
    BOOST_REQUIRE(query_.is_tx(tx->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__valid_twice__duplicate_transaction, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::duplicate_transaction);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__conflict__double_spend, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE_EQUAL(submit(package({ spend(parent_value) })).first, node::error::success);
    BOOST_REQUIRE_EQUAL(submit(package({ spend(sub1(parent_value)) })).first, system::error::double_spend);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__parent_and_child__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ first, second })).first, node::error::success);
    BOOST_REQUIRE(query_.is_tx(first->hash(false)));
    BOOST_REQUIRE(query_.is_tx(second->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__stored_parent_and_child__success_archived, chaser_transaction_pooling_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ first })).first, node::error::success);
    BOOST_REQUIRE_EQUAL(submit(package({ first, second })).first, node::error::success);
    BOOST_REQUIRE(query_.is_tx(second->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__zero_fee_minimum_rate__insufficient_fee, chaser_transaction_fee_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value) }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::insufficient_fee);
    BOOST_REQUIRE_EQUAL(result.second, zero);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__sufficient_fee__success, chaser_transaction_fee_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    BOOST_REQUIRE_EQUAL(result.first, node::error::success);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit__internal_relative_lock__relative_time_locked, chaser_transaction_relative_setup_fixture)
{
    const auto first = spend(parent_value);
    const auto second = spend_relative(first->hash(false), parent_value, 1);
    const auto result = submit(package({ first, second }));
    BOOST_REQUIRE_EQUAL(result.first, system::error::relative_time_locked);
    BOOST_REQUIRE(!query_.is_tx(first->hash(false)));
}

// do_bump

BOOST_FIXTURE_TEST_CASE(chaser_transaction__organized__invalid_confirmed_top__suspended_pooling_disabled, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
    node_.notify(node::error::success, chases::organized{ node::header_t{ 42 } });
    BOOST_REQUIRE(suspended());
    BOOST_REQUIRE_EQUAL(submit(package({ spend(parent_value) })).first, node::error::pooling_disabled);
}

// do_reorganized

BOOST_FIXTURE_TEST_CASE(chaser_transaction__reorganized__unpooled_block_tx__pooled, chaser_transaction_pooling_setup_fixture)
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block;
    const chain::input null{ chain::point{}, chain::script{}, max_uint32 };
    const chain::transaction coinbase{ 1u, chain::inputs{ null }, chain::outputs{ { 42u, chain::script{} } }, 0u };
    const auto tx = spend(parent_value);
    const chain::header header{ 1u, genesis.hash(), null_hash, genesis.header().timestamp() + 1u, genesis.header().bits(), 0u };
    const chain::block block{ header, { coinbase, *tx } };
    BOOST_REQUIRE(query_.set(block, database::context{ 0, 1, 0 }, {}, false, true));

    const auto link = query_.to_header(block.hash());
    BOOST_REQUIRE(query_.push_candidate(link));
    BOOST_REQUIRE(query_.push_confirmed(link, true));
    BOOST_REQUIRE(query_.pop_confirmed());

    const auto stored = query_.to_tx(tx->hash(false));
    BOOST_REQUIRE(!query_.is_pooled(stored));

    node_.notify(node::error::success, chases::reorganized{ link });
    BOOST_REQUIRE(pooled(stored));
    BOOST_REQUIRE(!query_.is_pooled(query_.to_tx(coinbase.hash(false))));

    database::table::pool::record record{};
    BOOST_REQUIRE(store_.pool.get(store_.pool.first(stored), record));
    BOOST_REQUIRE_EQUAL(record.time, header.timestamp());
}

// submit_compact

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__invalid_tx_link__integrity_suspended, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto result = submit_compact(package({}), { database::tx_link{ 42 } }, link);
    BOOST_REQUIRE_EQUAL(result.first, database::error::integrity);
    BOOST_REQUIRE(node_.suspended());
    BOOST_REQUIRE(!query_.is_associated(link));
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__stored_unpooled_tx__pooled, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto tx = spend(parent_value);
    BOOST_REQUIRE(query_.set(*tx));

    const auto stored = query_.to_tx(tx->hash(false));
    BOOST_REQUIRE(!query_.is_pooled(stored));

    const auto result = submit_compact(package({ tx }), { database::tx_link{ 0 }, database::tx_link::terminal }, link);
    BOOST_REQUIRE_EQUAL(result.first, node::error::success);
    BOOST_REQUIRE(query_.is_associated(link));
    BOOST_REQUIRE(query_.is_pooled(stored));
    BOOST_REQUIRE_EQUAL(query_.to_tx(tx->hash(false)), stored);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(chaser_transaction_scan_tests)

// BIP352 send_and_receive_test_vectors.json: "Simple send: two inputs".
static chain::input to_input(const hash_digest& hash, const data_chunk& script,
    const data_chunk& prevout) NOEXCEPT
{
    chain::input in{ { hash, 0 }, { script, false }, {}, max_uint32 };
    const chain::script prevout_script{ prevout, false };
    in.prevout = to_shared<chain::output>(0u, prevout_script);
    return in;
}

static const chain::transaction simple_send
{
    2u,
    chain::inputs
    {
        to_input
        (
            base16_hash("f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16"),
            base16_chunk("483046022100ad79e6801dd9a8727f342f31c71c4912866f59dc6e7981878e92c5844a0ce929022100fb0d2393e813968648b9753b7e9871d90ab3d815ebf91820d704b19f4ed224d621025a1e61f898173040e20616d43e9f496fba90338a39faa1ed98fcbaeee4dd9be5"),
            base16_chunk("76a91419c2f3ae0ca3b642bd3e49598b8da89f50c1416188ac")
        ),
        to_input
        (
            base16_hash("a1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"),
            base16_chunk("48304602210086783ded73e961037e77d49d9deee4edc2b23136e9728d56e4491c80015c3a63022100fda4c0f21ea18de29edbce57f7134d613e044ee150a89e2e64700de2d4e83d4e2103bd85685d03d111699b15d046319febe77f8de5286e9e512703cdee1bf3be3792"),
            base16_chunk("76a914d9317c66f54ff0a152ec50b1d19c25be50c8e15988ac")
        )
    },
    chain::outputs
    {
        { 0u, chain::script{ base16_chunk("51203e9fce73d4e77a4809908e3c3a2e54ee147b9312dc5044a193d1fc85de46e3c1"), false } }
    },
    0u
};

// Mainnet block 1 confirmed (coinbase link 1), scan rows linked 1 and 5.
static bool stored_scan(node::query& query) NOEXCEPT
{
    const auto& block1 = p2p_compact_setup_fixture::block1();
    const database::context context1{ 0, 1, 1231006505 };
    const auto work1 = block1.header().proof() * 2u;
    return query.set(block1, context1, work1, false, false)
        && query.push_confirmed(1, true)
        && query.set_silent(1, simple_send)
        && query.set_silent(5, simple_send);
}

struct chaser_transaction_scan_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_scan_setup_fixture(bool relay, bool current,
        const initializer& setup=stored_scan)
      : chaser_transaction_setup_fixture([=](configuration& config)
        {
            config.network.enable_relay = relay;
            config.node.currency_window_minutes = current ? 0 : 1440;
            config.node.silent_start_height = 0;
        }, setup)
    {
    }

    bool mapped(const database::header_link& link)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (!query_.get_scan_start(link).is_terminal())
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

};

struct scan_current_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_current_fixture()
      : chaser_transaction_scan_setup_fixture(false, true)
    {
    }
};

struct scan_pooling_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_pooling_fixture()
      : chaser_transaction_scan_setup_fixture(true, true)
    {
    }
};

struct scan_stale_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_stale_fixture()
      : chaser_transaction_scan_setup_fixture(false, false)
    {
    }
};

struct scan_complete_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_complete_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query)
                && query.set_scan_start(0, 42)
                && query.set_scan_start(1, 43);
        })
    {
    }
};

struct scan_partial_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_partial_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query) && query.set_scan_start(1, 43);
        })
    {
    }
};

struct scan_unextended_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_unextended_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query) && query.set_scan_start(0, 0);
        })
    {
    }
};

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__current_not_pooling__mapped, scan_current_fixture)
{
    BOOST_REQUIRE(mapped(0));
    BOOST_REQUIRE(!query_.is_pooling());
    BOOST_REQUIRE_EQUAL(query_.get_scan_frontier(), 0u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__current_pooling__mapped_and_latched, scan_pooling_fixture)
{
    BOOST_REQUIRE(mapped(0));
    BOOST_REQUIRE(query_.is_pooling());
    BOOST_REQUIRE_EQUAL(query_.get_scan_frontier(), 2u);
    BOOST_REQUIRE_EQUAL(query_.get_latch_tx(), 2u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__not_current__unmapped, scan_stale_fixture)
{
    BOOST_REQUIRE(query_.get_scan_start(0).is_terminal());
    BOOST_REQUIRE(query_.get_scan_start(1).is_terminal());
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__complete_map__unchanged, scan_complete_fixture)
{
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(0), 42u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(1), 43u);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__partial_map__remapped, scan_partial_fixture)
{
    BOOST_REQUIRE(mapped(0));
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BOOST_FIXTURE_TEST_CASE(chaser_transaction__scan__unmapped_top__extended, scan_unextended_fixture)
{
    BOOST_REQUIRE(mapped(1));
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BOOST_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(protocol_transaction_tests)

// protocol_transaction_in

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__unknown_tx__get_data, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);

    const auto payload = receive(peer::get_data::command);
    const auto message = peer::get_data::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().is_transaction_type());
    BOOST_REQUIRE(message->items.front().hash == one_hash);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__stored_tx__no_get_data, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, parent().hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__not_current__no_get_data, chaser_transaction_relay_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__unrequested__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::transaction{ spend(parent_value) }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__unrequested__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__requested__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::transaction, one_hash } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::not_found{ { { type_id::transaction, one_hash } } }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_valid__archived, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BOOST_REQUIRE(archived(tx->hash(false)));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_missing_prevout__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto missing = spend(missing_hash, parent_value);
    const auto valid = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, missing->hash(false) }, { type_id::transaction, valid->hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ missing }, node_version->value);
    send(peer::transaction{ valid }, node_version->value);
    BOOST_REQUIRE(archived(valid->hash(false)));
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_overspent__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(add1(parent_value));
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__block_only__no_get_data, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::inventory{ { { type_id::block, one_hash } } }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__inventory__excessive_backlog__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(announcement(peer::max_inventory, 1), node_version->value);
    send(announcement(1, 2), node_version->value);
    send(announcement(1, 3), node_version->value);
    send(peer::ping{ 42 }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::pong::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__not_found__block_only__not_stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::not_found{ { { type_id::block, one_hash } } }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::get_data::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_insufficient_fee_70013__stopped, chaser_transaction_fee_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto tx = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, tx->hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ tx }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_in__transaction__requested_insufficient_fee_70001__not_stopped, chaser_transaction_fee_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::bip37, true));

    const auto low = spend(parent_value);
    const auto high = spend(parent_value - 1'000);
    send(peer::inventory{ { { type_id::transaction, low->hash(false) }, { type_id::transaction, high->hash(false) } } }, peer::level::bip37);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ low }, peer::level::bip37);
    send(peer::transaction{ high }, peer::level::bip37);
    BOOST_REQUIRE(archived(high->hash(false)));
    BOOST_REQUIRE(!query_.is_tx(low->hash(false)));
}

// protocol_transaction_out

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__start__minimum_fee_rate__fee_filter, chaser_transaction_fee_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto payload = receive(peer::fee_filter::command);
    const auto message = peer::fee_filter::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->minimum_fee, 1'526u);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__start__zero_minimum_fee_rate__no_fee_filter, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BOOST_REQUIRE(!received_before_pong(peer::fee_filter::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__submit__archived__inventory, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == type_id::transaction);
    BOOST_REQUIRE(message->items.front().hash == tx->hash(false));
}

////BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__submit__below_fee_filter__not_announced, chaser_transaction_pooling_setup_fixture)
////{
////    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

////    send(peer::fee_filter{ 1'000'000 }, node_version->value);
////    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

////    const auto low = spend(parent_value);
////    BOOST_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);

////    send(peer::fee_filter{ 0 }, node_version->value);
////    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

////    const auto next = spend(low->hash(false), parent_value);
////    BOOST_REQUIRE_EQUAL(submit(package({ next })).first, node::error::success);

////    const auto payload = receive(peer::inventory::command);
////    const auto message = peer::inventory::deserialize(node_version->value, payload);
////    BOOST_REQUIRE(message);
////    BOOST_REQUIRE_EQUAL(message->items.size(), one);
////    BOOST_REQUIRE(message->items.front().hash == next->hash(false));
////}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__submit__announced_by_peer__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto first = spend(parent_value);
    send(peer::inventory{ { { type_id::transaction, first->hash(false) } } }, node_version->value);
    BOOST_REQUIRE(!receive(peer::get_data::command).empty());

    send(peer::transaction{ first }, node_version->value);
    BOOST_REQUIRE(archived(first->hash(false)));

    const auto next = spend(first->hash(false), parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ next })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == next->hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__stored_tx__transaction, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::transaction, parent().hash(false) } } }, node_version->value);

    const auto payload = receive(peer::transaction::command);
    const auto message = peer::transaction::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->transaction_ptr->hash(false) == parent().hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__witness_not_provided__stopped, chaser_transaction_no_witness_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::witness_tx, parent().hash(false) } } }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::transaction::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__unknown_tx_not_found_disabled__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::get_data{ { { type_id::transaction, one_hash } } }, node_version->value);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__block_then_stored_tx__transaction, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(peer::get_data{ { { type_id::block, genesis.hash() }, { type_id::transaction, parent().hash(false) } } }, node_version->value);

    const auto message = peer::transaction::deserialize(node_version->value, receive_before_pong(peer::transaction::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->transaction_ptr->hash(false) == parent().hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__transaction_event__missing_tx_70001__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::bip37, true));
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

    node_.notify(node::error::success, chases::transaction{ node::transaction_t{ 42 } });

    // The announcement of a subsequent tx orders the missing tx event.
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == tx->hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__transaction_event__missing_tx_70013__stopped_suspended, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

    node_.notify(node::error::success, chases::transaction{ node::transaction_t{ 42 } });
    BOOST_REQUIRE(suspended());
    BOOST_REQUIRE_THROW(receive(peer::pong::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__stale_event__became_current__fee_filter, chaser_transaction_relay_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    const auto initial = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BOOST_REQUIRE(initial);
    BOOST_REQUIRE_EQUAL(initial->minimum_fee, config_.bitcoin.max_money());

    config_.node.currency_window_minutes = 0;
    node_.notify(node::error::success, chases::stale{});

    const auto current = peer::fee_filter::deserialize(node_version->value, await_before_pong(peer::fee_filter::command));
    BOOST_REQUIRE(current);
    BOOST_REQUIRE_EQUAL(current->minimum_fee, 0u);
}

// memory_pool

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__disabled__no_inventory, chaser_transaction_pooling_setup_fixture)
{
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__empty__no_inventory, chaser_transaction_memory_pool_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__pooled__inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto parent_tx = spend(parent_value);
    const auto child_tx = spend(parent_tx->hash(false), parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ parent_tx, child_tx })).first, node::error::success);
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::memory_pool{}, node_version->value);

    const auto message = peer::inventory::deserialize(node_version->value, receive_before_pong(peer::inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), two);
    BOOST_REQUIRE(message->items.front().type == type_id::transaction);
    BOOST_REQUIRE(message->items.front().hash == parent_tx->hash(false));
    BOOST_REQUIRE(message->items.back().type == type_id::transaction);
    BOOST_REQUIRE(message->items.back().hash == child_tx->hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__bip35__inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BOOST_REQUIRE(handshake(0, peer::level::bip35, true));

    send(peer::memory_pool{}, peer::level::bip35);

    const auto message = peer::inventory::deserialize(peer::level::bip35, receive_before_pong(peer::inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == type_id::transaction);
    BOOST_REQUIRE(message->items.front().hash == tx->hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__below_fee_filter__no_inventory, chaser_transaction_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 1'000 }, node_version->value);
    send(peer::memory_pool{}, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__memory_pool__witness_relay__wtxid_inventory, chaser_transaction_witness_memory_pool_setup_fixture)
{
    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);
    BOOST_REQUIRE(handshake(0, peer::level::bip339, true, true));

    send(peer::memory_pool{}, node_version->value);

    const auto message = peer::inventory::deserialize(node_version->value, receive_before_pong(peer::inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == type_id::wtxid);
    BOOST_REQUIRE(message->items.front().hash == tx->hash(true));
}

BOOST_AUTO_TEST_SUITE_END()
