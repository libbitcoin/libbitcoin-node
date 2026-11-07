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
#include <algorithm>
#include <chrono>
#include <ctime>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

using namespace system;

static const system::settings confirm_settings{ chain::selection::mainnet };
static const uint64_t confirm_subsidy = confirm_settings.initial_subsidy();

static const chain::block& confirm_genesis()
{
    static const system::settings bitcoin{ chain::selection::mainnet };
    return bitcoin.genesis_block;
}

static chain::transaction confirm_coinbase(uint8_t tag)
{
    const chain::point null{ null_hash, chain::point::null_index };
    const chain::script tagged{ data_chunk{ 0x01, tag }, false };
    const chain::script op_true{ data_chunk{ 0x51 }, false };
    const chain::input input{ null, tagged, max_uint32 };
    const chain::output output{ confirm_subsidy, op_true };
    return { 1u, chain::inputs{ input }, chain::outputs{ output }, 0u };
}

static chain::transaction confirm_spend(const chain::block& block)
{
    const auto prior = block.transactions_ptr()->front()->hash(false);
    const chain::point point{ prior, 0 };
    const chain::script op_true{ data_chunk{ 0x51 }, false };
    const chain::input input{ point, chain::script{}, max_uint32 };
    const chain::output output{ confirm_subsidy, op_true };
    return { 1u, chain::inputs{ input }, chain::outputs{ output }, 0u };
}

static chain::block confirm_block(const chain::block& parent, uint32_t height,
    const chain::transactions& txs)
{
    const auto& genesis = confirm_genesis().header();
    hashes ids{};
    for (const auto& tx: txs)
        ids.push_back(tx.hash(false));

    const auto previous = parent.hash();
    const auto root = sha256::merkle_root(std::move(ids));
    const auto timestamp = genesis.timestamp() + height * 600u;
    const auto bits = genesis.bits();
    const chain::header header{ 1u, previous, root, timestamp, bits, height };
    return { header, txs };
}

static const chain::block& confirm_c1()
{
    const auto& parent = confirm_genesis();
    const auto coinbase = confirm_coinbase(11);
    static const auto instance = confirm_block(parent, 1, { coinbase });
    return instance;
}

static const chain::block& confirm_c2()
{
    const auto& parent = confirm_c1();
    const auto coinbase = confirm_coinbase(12);
    static const auto instance = confirm_block(parent, 2, { coinbase });
    return instance;
}

static const chain::block& confirm_d1()
{
    const auto& parent = confirm_genesis();
    const auto coinbase = confirm_coinbase(21);
    static const auto instance = confirm_block(parent, 1, { coinbase });
    return instance;
}

static const chain::block& confirm_d2()
{
    const auto& parent = confirm_d1();
    const auto coinbase = confirm_coinbase(22);
    static const auto instance = confirm_block(parent, 2, { coinbase });
    return instance;
}

static const chain::block& confirm_e2()
{
    const auto& parent = confirm_d1();
    const auto coinbase = confirm_coinbase(23);
    const auto spend = confirm_spend(parent);
    static const auto instance = confirm_block(parent, 2, { coinbase, spend });
    return instance;
}

static const chain::block& confirm_e3()
{
    const auto& parent = confirm_d2();
    const auto coinbase = confirm_coinbase(24);
    const auto spend = confirm_spend(confirm_d1());
    static const auto instance = confirm_block(parent, 3, { coinbase, spend });
    return instance;
}

static bool is_valid_only(database::block_state state)
{
    return database::is_validated(state) && !database::is_confirmable(state);
}

////static chain::block confirm_recent()
////{
////    const auto& genesis = confirm_genesis().header();
////    const auto coinbase = confirm_coinbase(31);
////    const chain::header header{ 1u, genesis.hash(), sha256::merkle_root({ coinbase.hash(false) }), static_cast<uint32_t>(std::time(nullptr) - 55), genesis.bits(), 1u };
////    return { header, { coinbase } };
////}

static bool confirm_store(node::query& query, const chain::block& block,
    uint32_t height)
{
    const auto& genesis = confirm_genesis().header();
    const database::context context{ 0, height, genesis.timestamp() };
    const auto work = genesis.proof() * add1(height);
    return query.set(block, context, work, false, false);
}

static bool confirm_candidate(node::query& query, const chain::block& block,
    uint32_t height)
{
    return confirm_store(query, block, height) &&
        query.push_candidate(query.to_header(block.hash()));
}

static bool confirm_confirmed(node::query& query, const chain::block& block,
    uint32_t height)
{
    return confirm_store(query, block, height) &&
        query.set_block_confirmable(query.to_header(block.hash())) &&
        query.push_confirmed(query.to_header(block.hash()), true);
}

// An empty filter body stands in for one computed from populated prevouts.
static bool confirm_filtered(node::query& query, const chain::block& block)
{
    return query.set_filter_body(query.to_header(block.hash()), data_chunk{});
}

// The prevout cache the validate chaser leaves for confirmation.
static bool confirm_prevouts(node::query& query, const chain::block& block)
{
    data_chunk prevouts{};
    node::query::prevout_spends spends{};
    database::tx_links conflicts{};
    const auto link = query.to_header(block.hash());
    return query.get_block_prevouts(prevouts, spends, conflicts, link) &&
        query.set_prevouts(link, spends, conflicts);
}

// A candidate in the states the validate chaser leaves for confirmation.
static bool confirm_validated(node::query& query, const chain::block& block,
    uint32_t height)
{
    return confirm_candidate(query, block, height) &&
        confirm_prevouts(query, block) &&
        confirm_filtered(query, block) &&
        query.set_block_scanned(query.to_header(block.hash())) &&
        query.set_block_valid(query.to_header(block.hash()));
}

// A milestone candidate, confirmed without validation (bypass).
static bool confirm_milestoned(node::query& query, const chain::block& block,
    uint32_t height)
{
    const auto& genesis = confirm_genesis().header();
    const database::context context{ 0, height, genesis.timestamp() };
    const auto work = genesis.proof() * add1(height);
    return query.set(block, context, work, true, false) &&
        query.push_candidate(query.to_header(block.hash())) &&
        confirm_filtered(query, block);
}

static bool confirm_stronger(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) &&
        confirm_validated(query, confirm_d1(), 1) &&
        confirm_validated(query, confirm_d2(), 2);
}

static bool confirm_weaker(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) &&
        confirm_confirmed(query, confirm_c2(), 2) &&
        confirm_validated(query, confirm_d1(), 1);
}

static bool confirm_tied(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) &&
        confirm_validated(query, confirm_d1(), 1);
}

static bool confirm_immature(node::query& query)
{
    return confirm_validated(query, confirm_d1(), 1) &&
        confirm_validated(query, confirm_e2(), 2);
}

static bool confirm_immature_fork(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) && confirm_immature(query);
}

static bool confirm_immature_deep_fork(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) &&
        confirm_confirmed(query, confirm_c2(), 2) &&
        confirm_validated(query, confirm_d1(), 1) &&
        confirm_validated(query, confirm_d2(), 2) &&
        confirm_validated(query, confirm_e3(), 3);
}

static bool confirm_bypass_then_validated(node::query& query)
{
    return confirm_milestoned(query, confirm_d1(), 1) &&
        confirm_validated(query, confirm_d2(), 2);
}

static bool confirm_unfiltered(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) &&
        query.set_block_scanned(query.to_header(confirm_d1().hash())) &&
        query.set_block_valid(query.to_header(confirm_d1().hash()));
}

static bool confirm_candidates(node::query& query)
{
    return confirm_validated(query, confirm_d1(), 1) &&
        confirm_validated(query, confirm_d2(), 2);
}

static bool confirm_confirmable(node::query& query)
{
    return confirm_candidate(query, confirm_d1(), 1) &&
        confirm_filtered(query, confirm_d1()) &&
        query.set_block_confirmable(query.to_header(confirm_d1().hash()));
}

static bool confirm_confirmables(node::query& query)
{
    return confirm_confirmable(query) &&
        confirm_candidate(query, confirm_d2(), 2) &&
        confirm_filtered(query, confirm_d2()) &&
        query.set_block_confirmable(query.to_header(confirm_d2().hash()));
}

static bool confirm_headerless_confirmed(node::query& query)
{
    const auto& genesis = confirm_genesis().header();
    const database::context context{ 0, 1, genesis.timestamp() };
    const auto work = genesis.proof() * 2u;
    return query.set(confirm_c1().header(), context, work, false) &&
        query.push_confirmed(query.to_header(confirm_c1().hash()), false) &&
        confirm_confirmables(query);
}

static bool confirm_unheaded(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1) &&
        query.push_candidate(query.to_header(confirm_c1().hash())) &&
        confirm_candidate(query, confirm_c2(), 2) &&
        confirm_filtered(query, confirm_c2());
}

static bool confirm_unprevouted(node::query& query)
{
    return confirm_confirmable(query) &&
        confirm_candidate(query, confirm_e2(), 2) &&
        confirm_filtered(query, confirm_e2()) &&
        query.set_block_valid(query.to_header(confirm_e2().hash()));
}

static bool confirm_c1_confirmed(node::query& query)
{
    return confirm_confirmed(query, confirm_c1(), 1);
}

static bool confirm_genesis_only(node::query&)
{
    return true;
}

static void confirm_uncheckpointed(configuration& config)
{
    config.bitcoin.checkpoints.clear();
}

static void confirm_current(configuration& config)
{
    config.bitcoin.checkpoints.clear();
    config.node.currency_window_minutes = 30u * 365u * 24u * 60u;
}

// A session to observe wire broadcasts.
class wire_session
  : public network::session_manual
{
public:
    using session_manual::session_manual;
};

// A node that runs its network without starting its own chasers.
class harness_node
  : public full_node
{
public:
    using full_node::full_node;

protected:
    void do_start(const result_handler& handler) NOEXCEPT override
    {
        network::net::do_start(handler);
    }

    void do_run(const result_handler& handler) NOEXCEPT override
    {
        network::net::do_run(handler);
    }
};

class header_accessor
  : public chaser_header
{
public:
    using chaser_header::chaser_header;
    using chaser::strand;
};

class confirm_accessor
  : public chaser_confirm
{
public:
    using chaser_confirm::chaser_confirm;
    using chaser::strand;
};

// Records every chase event, and awaits a count of a given kind.
class chase_recorder
{
public:
    template <chase Kind>
    using payload_t = std::remove_cvref_t<decltype(to_payload<Kind>(std::declval<const event_value&>()))>;

    bool subscribe(full_node& node)
    {
        const auto subscribed = std::make_shared<std::promise<void>>();
        auto subscription = subscribed->get_future();
        node.subscribe_chase([this](const code&, event_value value) NOEXCEPT
        {
            std::lock_guard lock{ mutex_ };
            events_.push_back(value);
            return true;
        }, [subscribed](const code&, auto) NOEXCEPT
        {
            subscribed->set_value();
        });

        return subscription.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    }

    size_t count(chase kind) const
    {
        std::lock_guard lock{ mutex_ };
        return std::count_if(events_.cbegin(), events_.cend(), [kind](const auto& value) NOEXCEPT { return to_chase(value) == kind; });
    }

    bool await(chase kind, size_t number=1) const
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (count(kind) < number)
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return true;
    }

    /// The recorded event kinds in order, for assertion messages.
    std::string dump() const
    {
        std::lock_guard lock{ mutex_ };
        std::string out{};
        for (const auto& value: events_)
            out += std::to_string(static_cast<int>(to_chase(value))) + " ";

        return out;
    }

    template <chase Kind>
    std::optional<payload_t<Kind>> payload(size_t number=1) const
    {
        std::lock_guard lock{ mutex_ };
        for (const auto& value: events_)
            if (to_chase(value) == Kind && is_zero(--number))
                return to_payload<Kind>(value);

        return {};
    }

private:
    mutable std::mutex mutex_;
    std::vector<event_value> events_{};
};

// A running node whose only chasers are the header and confirm chasers.
struct confirm_setup_fixture
{
    DELETE_COPY_MOVE(confirm_setup_fixture);

    using condition = std::function<bool()>;
    using initializer = std::function<bool(node::query&)>;
    using configurator = std::function<void(configuration&)>;

    static configuration configure(const configurator& configurer)
    {
        configuration config{ test::node_configuration() };
        config.database.path = TEST_DIRECTORY;
        config.network.path = TEST_DIRECTORY;
        config.network.inbound.connections = 0;
        config.network.inbound.binds.clear();
        config.network.outbound.connections = 0;
        config.network.outbound.seeds.clear();
        if (configurer)
            configurer(config);

        // The store envelope is derived from the configured chain.
        config.database.initialize(config.bitcoin, config.node.limited_blocks,
            config.node.silent_start_height);

        return config;
    }

    confirm_setup_fixture(const initializer& setup,
        const configurator& configurer)
      : config_{ configure(configurer) },
        store_{ config_.database },
        query_{ store_ },
        node_{ query_, config_, log_ },
        header_{ node_ },
        confirm_{ node_ }
    {
        test::clear(test::directory);
        auto ec = store_.create([](auto, auto) {});
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        // A failed setup must release the store or every later test fails.
        const auto initialized = setup(query_);
        if (!initialized)
            store_.close([](auto, auto) {});

        BC_REQUIRE(initialized);

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT { started.set_value(ec); });
        ec = started.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(events_.subscribe(node_));

        std::promise<code> chased{};
        network::io::post(node_.strand(), [&]() NOEXCEPT
        {
            const auto headed = header_.start();
            chased.set_value(headed ? headed : confirm_.start());
        });

        ec = chased.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT { running.set_value(ec); });
        ec = running.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());

        // The node start event, which the node does not raise for the harness.
        node_.notify(node::error::success, chases::start{});
    }

    ~confirm_setup_fixture()
    {
        // The node stops only its own chasers.
        header_.stopping(network::error::service_stopped);
        confirm_.stopping(network::error::service_stopped);
        node_.close();
        const auto ec = store_.close([](auto, auto) {});
        BC_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    harness_node node_;
    header_accessor header_;
    confirm_accessor confirm_;
    chase_recorder events_{};

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

    /// Complete all work posted to the strand before this call.
    static void barrier(network::io::strand& strand)
    {
        std::promise<void> posted{};
        network::io::post(strand, [&]() NOEXCEPT { posted.set_value(); });
        BC_REQUIRE(posted.get_future().wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    }

    /// Complete event dispatch and the chaser work it posted.
    void settle()
    {
        barrier(node_.strand());
        barrier(confirm_.strand());
        barrier(header_.strand());
        barrier(node_.strand());
        barrier(confirm_.strand());
    }

    database::header_link link(const chain::block& block) const
    {
        return query_.to_header(block.hash());
    }

    bool confirmed(const chain::block& block, size_t height)
    {
        return await([&]()
        {
            return query_.to_confirmed(height) == link(block);
        });
    }

    bool candidate(const chain::block& block, size_t height) const
    {
        return query_.to_candidate(height) == link(block);
    }

    bool stated(const chain::block& block,
        bool(*expected)(database::block_state))
    {
        return await([&]()
        {
            return expected(query_.get_block_state(link(block)));
        });
    }
};

struct confirm_stronger_fixture
  : confirm_setup_fixture
{
    confirm_stronger_fixture()
      : confirm_setup_fixture(confirm_stronger, confirm_uncheckpointed)
    {
    }
};

struct confirm_weaker_fixture
  : confirm_setup_fixture
{
    confirm_weaker_fixture()
      : confirm_setup_fixture(confirm_weaker, confirm_uncheckpointed)
    {
    }
};

struct confirm_immature_fixture
  : confirm_setup_fixture
{
    confirm_immature_fixture()
      : confirm_setup_fixture(confirm_immature, confirm_uncheckpointed)
    {
    }
};

struct confirm_immature_fork_fixture
  : confirm_setup_fixture
{
    confirm_immature_fork_fixture()
      : confirm_setup_fixture(confirm_immature_fork, confirm_uncheckpointed)
    {
    }
};

struct confirm_tied_fixture
  : confirm_setup_fixture
{
    confirm_tied_fixture()
      : confirm_setup_fixture(confirm_tied, confirm_uncheckpointed)
    {
    }
};

struct confirm_immature_deep_fork_fixture
  : confirm_setup_fixture
{
    confirm_immature_deep_fork_fixture()
      : confirm_setup_fixture(confirm_immature_deep_fork, confirm_uncheckpointed)
    {
    }
};

struct confirm_bypass_fixture
  : confirm_setup_fixture
{
    confirm_bypass_fixture()
      : confirm_setup_fixture(confirm_bypass_then_validated, confirm_uncheckpointed)
    {
    }
};

struct confirm_unfiltered_fixture
  : confirm_setup_fixture
{
    confirm_unfiltered_fixture()
      : confirm_setup_fixture(confirm_unfiltered, confirm_uncheckpointed)
    {
    }
};

struct confirm_current_fixture
  : confirm_setup_fixture
{
    confirm_current_fixture()
      : confirm_setup_fixture(confirm_candidates, [](configuration& config)
        {
            config.bitcoin.checkpoints.clear();
            config.node.currency_window_minutes = 30u * 365u * 24u * 60u;
        })
    {
    }
};

////struct confirm_windowed_fixture
////  : confirm_setup_fixture
////{
////    confirm_windowed_fixture()
////      : confirm_setup_fixture([](node::query&) { return true; }, [](configuration& config)
////        {
////            config.bitcoin.checkpoints.clear();
////            config.node.currency_window_minutes = 1;
////        })
////    {
////    }

////    bool subscribe(const std::shared_ptr<std::promise<void>>& heard, chase awaited)
////    {
////        const auto subscribed = std::make_shared<std::promise<void>>();
////        auto subscription = subscribed->get_future();
////        node_.subscribe_chase([heard, awaited](const code&, event_value value) NOEXCEPT
////        {
////            if (to_chase(value) != awaited)
////                return true;

////            heard->set_value();
////            return false;
////        }, [subscribed](const code&, auto) NOEXCEPT
////        {
////            subscribed->set_value();
////        });

////        return subscription.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
////    }
////};

struct confirm_confirmable_fixture
  : confirm_setup_fixture
{
    confirm_confirmable_fixture()
      : confirm_setup_fixture(confirm_confirmable, confirm_uncheckpointed)
    {
    }
};

struct confirm_c1_confirmed_fixture
  : confirm_setup_fixture
{
    confirm_c1_confirmed_fixture()
      : confirm_setup_fixture(confirm_c1_confirmed, confirm_uncheckpointed)
    {
    }
};

struct confirm_headerless_confirmed_fixture
  : confirm_setup_fixture
{
    confirm_headerless_confirmed_fixture()
      : confirm_setup_fixture(confirm_headerless_confirmed, confirm_uncheckpointed)
    {
    }
};

struct confirm_unheaded_fixture
  : confirm_setup_fixture
{
    confirm_unheaded_fixture()
      : confirm_setup_fixture(confirm_unheaded, {})
    {
    }
};

struct confirm_unprevouted_fixture
  : confirm_setup_fixture
{
    confirm_unprevouted_fixture()
      : confirm_setup_fixture(confirm_unprevouted, confirm_uncheckpointed)
    {
    }
};

struct confirm_genesis_fixture
  : confirm_setup_fixture
{
    confirm_genesis_fixture()
      : confirm_setup_fixture(confirm_genesis_only, confirm_current)
    {
    }
};

BC_AUTO_TEST_SUITE(chaser_confirm_tests)

BC_FIXTURE_TEST_CASE(chaser_confirm__start__stronger_candidate_fork__reorganized, confirm_stronger_fixture)
{
    BC_REQUIRE(confirmed(confirm_d2(), 2));
    BC_REQUIRE(confirmed(confirm_d1(), 1));
    BC_REQUIRE(stated(confirm_d1(), database::is_confirmable));
    BC_REQUIRE(stated(confirm_d2(), database::is_confirmable));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_c1())));
    BC_REQUIRE(query_.is_strong_block(link(confirm_d2())));
    BC_REQUIRE(!query_.is_strong_block(link(confirm_c1())));

    // The confirmed c1 is popped before the fork is pushed.
    BC_REQUIRE_MESSAGE(events_.await(chase::organized, 2), events_.dump());
    BC_REQUIRE_EQUAL(events_.count(chase::reorganized), 1u);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>()->link, link(confirm_c1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(1)->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(2)->link, link(confirm_d2()).value);
    BC_REQUIRE_EQUAL(events_.count(chase::confirmable), 2u);
    BC_REQUIRE_EQUAL(events_.count(chase::unconfirmable), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__weaker_candidate_fork__not_reorganized, confirm_weaker_fixture)
{
    settle();
    BC_REQUIRE(stated(confirm_d1(), is_valid_only));
    BC_REQUIRE(confirmed(confirm_c2(), 2));
    BC_REQUIRE(confirmed(confirm_c1(), 1));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d1())));
    BC_REQUIRE_EQUAL(events_.count(chase::reorganized), 0u);
    BC_REQUIRE_EQUAL(events_.count(chase::organized), 0u);

    // The fork becomes stronger when extended, and then reorganizes.
    BC_REQUIRE(confirm_validated(query_, confirm_d2(), 2));
    BC_REQUIRE(confirm_validated(query_, confirm_e3(), 3));
    node_.notify(node::error::success, chases::bump{ 0 });
    BC_REQUIRE_MESSAGE(events_.await(chase::unconfirmable), events_.dump());
    BC_REQUIRE(stated(confirm_e3(), database::is_unconfirmable));
    BC_REQUIRE_MESSAGE(events_.await(chase::organized, 4), events_.dump());
    BC_REQUIRE(confirmed(confirm_c2(), 2));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d2())));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__tied_candidate_fork__reorganized, confirm_tied_fixture)
{
    BC_REQUIRE(confirmed(confirm_d1(), 1));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_c1())));
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
    BC_REQUIRE_MESSAGE(events_.await(chase::reorganized), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>()->link, link(confirm_c1()).value);
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__immature_spend__rolled_back_to_fork_point, confirm_immature_fixture)
{
    BC_REQUIRE(stated(confirm_e2(), database::is_unconfirmable));
    BC_REQUIRE_MESSAGE(events_.await(chase::unconfirmable), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::unconfirmable>()->link, link(confirm_e2()).value);

    // The valid d1 is organized and then rolled back with the failed e2.
    BC_REQUIRE_MESSAGE(events_.await(chase::reorganized), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>()->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>()->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 0u);
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d1())));
    BC_REQUIRE(stated(confirm_d1(), database::is_confirmable));

    // The header chaser pops both candidates, retaining d1 for extension.
    BC_REQUIRE_MESSAGE(events_.await(chase::disorganized), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 0u);
    BC_REQUIRE(await([&]() { return query_.get_top_candidate() == 0u; }));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__immature_spend_fork__rolled_back, confirm_immature_fork_fixture)
{
    BC_REQUIRE(stated(confirm_e2(), database::is_unconfirmable));
    BC_REQUIRE_MESSAGE(events_.await(chase::unconfirmable), events_.dump());
    BC_REQUIRE(confirmed(confirm_c1(), 1));
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d1())));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_e2())));
    BC_REQUIRE(query_.is_strong_block(link(confirm_c1())));
    BC_REQUIRE(!query_.is_strong_block(link(confirm_d1())));

    // Pop c1, push d1, fail e2, pop d1, push c1.
    BC_REQUIRE_EQUAL(events_.count(chase::reorganized), 2u);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(1)->link, link(confirm_c1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(2)->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(1)->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(2)->link, link(confirm_c1()).value);
    BC_REQUIRE_EQUAL(events_.count(chase::unconfirmable), 1u);

    // The header chaser resets the candidate chain to the confirmed chain.
    BC_REQUIRE_MESSAGE(events_.await(chase::disorganized), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 1u);
    BC_REQUIRE(await([&]() { return query_.get_top_candidate() == 1u; }));
    BC_REQUIRE(candidate(confirm_c1(), 1));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__immature_spend_deep_fork__rolled_back, confirm_immature_deep_fork_fixture)
{
    BC_REQUIRE(stated(confirm_e3(), database::is_unconfirmable));
    BC_REQUIRE_MESSAGE(events_.await(chase::organized, 4), events_.dump());
    BC_REQUIRE(confirmed(confirm_c2(), 2));
    BC_REQUIRE(confirmed(confirm_c1(), 1));
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 2u);
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d1())));
    BC_REQUIRE(!query_.is_confirmed_block(link(confirm_d2())));
    BC_REQUIRE(stated(confirm_d1(), database::is_confirmable));
    BC_REQUIRE(stated(confirm_d2(), database::is_confirmable));

    // Pop c2, c1, push d1, d2, fail e3, pop d2, d1, push c1, c2.
    BC_REQUIRE_EQUAL(events_.count(chase::reorganized), 4u);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(1)->link, link(confirm_c2()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(2)->link, link(confirm_c1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(3)->link, link(confirm_d2()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::reorganized>(4)->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(1)->link, link(confirm_d1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(2)->link, link(confirm_d2()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(3)->link, link(confirm_c1()).value);
    BC_REQUIRE_EQUAL(events_.payload<chase::organized>(4)->link, link(confirm_c2()).value);
    BC_REQUIRE_MESSAGE(events_.await(chase::disorganized), events_.dump());
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 2u);
    BC_REQUIRE(await([&]() { return query_.get_top_candidate() == 2u; }));
    BC_REQUIRE(candidate(confirm_c2(), 2));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__bypass_candidate__organized_unvalidated, confirm_bypass_fixture)
{
    BC_REQUIRE(confirmed(confirm_d2(), 2));
    BC_REQUIRE(confirmed(confirm_d1(), 1));
    BC_REQUIRE(query_.is_bypass(link(confirm_d1())));
    BC_REQUIRE(!query_.is_confirmable(link(confirm_d1())));
    BC_REQUIRE(stated(confirm_d2(), database::is_confirmable));
    BC_REQUIRE(query_.is_filtered_head(link(confirm_d1())));
    BC_REQUIRE_MESSAGE(events_.await(chase::confirmable, 2), events_.dump());
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__unfiltered_candidate__not_organized, confirm_unfiltered_fixture)
{
    settle();
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 0u);
    BC_REQUIRE(stated(confirm_d1(), is_valid_only));
    BC_REQUIRE_EQUAL(events_.count(chase::organized), 0u);

    // Filtering completes the candidate, which is then organized on bump.
    BC_REQUIRE(confirm_filtered(query_, confirm_d1()));
    node_.notify(node::error::success, chases::bump{ 0 });
    BC_REQUIRE(confirmed(confirm_d1(), 1));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__bump__suspended__not_organized_until_resumed, confirm_unfiltered_fixture)
{
    settle();
    node_.suspend(node::error::suspended_channel);
    BC_REQUIRE(node_.suspended());
    BC_REQUIRE(confirm_filtered(query_, confirm_d1()));
    node_.notify(node::error::success, chases::bump{ 0 });
    settle();
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 0u);

    BC_REQUIRE(node_.resume());
    BC_REQUIRE(confirmed(confirm_d1(), 1));
}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__current_candidates__organized, confirm_current_fixture)
{
    BC_REQUIRE(confirmed(confirm_d2(), 2));
    BC_REQUIRE(stated(confirm_d2(), database::is_confirmable));
    BC_REQUIRE_MESSAGE(events_.await(chase::block, 2), events_.dump());
}

////BC_FIXTURE_TEST_CASE(chaser_confirm__bump__recent_confirmable_candidate__organized_stale, confirm_windowed_fixture)
////{
////    const auto block = confirm_recent();
////    const auto heard = std::make_shared<std::promise<void>>();
////    auto hearing = heard->get_future();
////    BC_REQUIRE(subscribe(heard, chase::stale));
////    BC_REQUIRE(confirm_store(query_, block, 1) && query_.push_candidate(link(block)) && query_.set_block_confirmable(link(block)));
////    node_.notify(node::error::success, chases::bump{ 0 });
////    BC_REQUIRE(confirmed(block, 1));
////    BC_REQUIRE(hearing.wait_for(std::chrono::seconds(30)) == std::future_status::ready);
////}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__confirmable_candidate__organized, confirm_confirmable_fixture)
{
    BC_REQUIRE(confirmed(confirm_d1(), 1));
    BC_REQUIRE(!node_.suspended());
}

BC_FIXTURE_TEST_CASE(chaser_confirm__bump__unstored_confirmed_top__suspended, confirm_c1_confirmed_fixture)
{
    BC_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
    BC_REQUIRE(confirm_candidate(query_, confirm_d1(), 1));
    BC_REQUIRE(confirm_filtered(query_, confirm_d1()));
    BC_REQUIRE(query_.set_block_valid(link(confirm_d1())));
    node_.notify(node::error::success, chases::bump{ 0 });
    BC_REQUIRE(await([&]() { return node_.suspended(); }));
    BC_REQUIRE(confirmed(confirm_c1(), 1));
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 2u);
}

////BC_FIXTURE_TEST_CASE(chaser_confirm__start__headerless_confirmed_stronger_fork__suspended, confirm_headerless_confirmed_fixture)
////{
////    BC_REQUIRE(await([&]() { return node_.suspended(); }));
////    BC_REQUIRE(confirmed(confirm_c1(), 1));
////    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
////}

BC_FIXTURE_TEST_CASE(chaser_confirm__start__bypassed_candidate_unheaded_parent__suspended, confirm_unheaded_fixture)
{
    BC_REQUIRE(await([&]() { return node_.suspended(); }));
    BC_REQUIRE(confirmed(confirm_c1(), 1));
    BC_REQUIRE_EQUAL(query_.get_top_confirmed(), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_confirm__bump__wire_block_demanded_current__broadcast_block, confirm_genesis_fixture)
{
    std::promise<network::wire_block::cptr> promise{};
    auto broadcast = promise.get_future();
    const auto session = std::make_shared<wire_session>(node_, 42);
    session->subscribe<network::wire_block>([&](const code&, const network::wire_block::cptr& message, uint64_t) NOEXCEPT
    {
        promise.set_value(message);
        return false;
    }, 42);

    node_.demand_wire_block(true);
    BC_REQUIRE(confirm_confirmable(query_));

    node_.notify(node::error::success, chases::bump{ 0 });
    BC_REQUIRE(confirmed(confirm_d1(), 1));
    BC_REQUIRE(broadcast.wait_for(std::chrono::seconds(10)) == std::future_status::ready);

    const auto message = broadcast.get();
    BC_REQUIRE(message->organized());
    BC_REQUIRE(message->block()->to_data(true) == confirm_d1().to_data(true));
    BC_REQUIRE_EQUAL(message->transactions().size(), 1u);
    BC_REQUIRE(message->transactions().front()->tx()->to_data(true) == confirm_d1().transactions_ptr()->front()->to_data(true));
}

////BC_FIXTURE_TEST_CASE(chaser_confirm__start__valid_candidate_without_prevouts__suspended_unconfirmable, confirm_unprevouted_fixture)
////{
////    BC_REQUIRE(await([&]() { return node_.suspended(); }));
////    BC_REQUIRE(stated(confirm_e2(), database::is_unconfirmable));
////    BC_REQUIRE(await([&]() { return query_.get_top_confirmed() == zero; }));
////}

BC_AUTO_TEST_SUITE_END()
