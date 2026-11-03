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
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

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

static uint32_t now()
{
    return system::possible_narrow_and_sign_cast<uint32_t>(network::zulu_time());
}

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
        node.subscribe_chase([this](const code& ec, event_value value) NOEXCEPT
        {
            if (ec)
                return false;

            std::lock_guard lock{ mutex_ };
            events_.push_back(value);
            return true;
        }, [subscribed](const code&, auto) NOEXCEPT
        {
            subscribed->set_value();
        });

        return subscription.wait_for(await_limit) == std::future_status::ready;
    }

    size_t count(chase kind) const
    {
        std::lock_guard lock{ mutex_ };
        return std::count_if(events_.cbegin(), events_.cend(), [kind](const auto& value) NOEXCEPT { return to_chase(value) == kind; });
    }

    bool await(chase kind, size_t number=1) const
    {
        const auto deadline = std::chrono::steady_clock::now() + await_limit;
        while (count(kind) < number)
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return true;
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

// A running node whose only chaser is the header chaser under test.
struct chaser_header_setup_fixture
{
    DELETE_COPY_MOVE(chaser_header_setup_fixture);

    using configurator = std::function<void(configuration&)>;
    using result = std::pair<code, size_t>;

    explicit chaser_header_setup_fixture(const configurator& configure={})
      : config_{ configured(configure) },
        store_{ config_.database },
        query_{ store_ },
        node_{ query_, config_, log_ },
        header_{ node_ }
    {
        test::clear(test::directory);
        auto ec = store_.create([](auto, auto) {});
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(query_.initialize(config_.bitcoin.genesis_block));

        std::promise<code> started{};
        node_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        ec = started.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        BC_REQUIRE(events_.subscribe(node_));

        std::promise<code> chased{};
        network::io::post(node_.strand(), [&]() NOEXCEPT
        {
            chased.set_value(header_.start());
        });

        ec = chased.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());

        std::promise<code> running{};
        node_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        ec = running.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());
    }

    ~chaser_header_setup_fixture()
    {
        node_.close();
        const auto ec = store_.close([](auto, auto) {});
        BC_WARN_MESSAGE(!ec, ec.message());
        test::clear(test::directory);
    }

    static configuration configured(const configurator& configure)
    {
        configuration config{ test::node_configuration() };
        config.database.path = TEST_DIRECTORY;
        config.network.path = TEST_DIRECTORY;
        config.network.inbound.connections = 0;
        config.network.inbound.binds.clear();
        config.network.outbound.connections = 0;
        config.network.outbound.seeds.clear();
        config.bitcoin.checkpoints.clear();

        if (configure)
            configure(config);

        // The store envelope is derived from the configured chain.
        config.database.initialize(config.bitcoin, config.node.limited_blocks,
            config.node.silent_start_height);

        return config;
    }

    /// Organize an unproven header.
    result organize(const system::chain::header::cptr& header)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        header_.organize(header, [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        });

        BC_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Organize a proven header.
    result organize(const system::chain::header::cptr& header, bool milestone)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        header_.organize(header, milestone, [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        });

        BC_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Prioritize a cached branch.
    result prioritize(const system::hash_digest& hash)
    {
        const auto promise = std::make_shared<std::promise<result>>();
        auto future = promise->get_future();
        header_.prioritize(hash, [promise](const code& ec, size_t height) NOEXCEPT
        {
            promise->set_value({ ec, height });
        });

        BC_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
        return future.get();
    }

    /// Complete all work posted to the header chaser before this call.
    void flush()
    {
        const auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        header_.get_minimum_work([promise](const code&, const uint256_t&) NOEXCEPT
        {
            promise->set_value();
        });

        BC_REQUIRE(future.wait_for(await_limit) == std::future_status::ready);
    }

    /// Notify the event and wait (bounded) for one more of the awaited event.
    bool notify(const event_value& event, chase awaited)
    {
        const auto expected = add1(events_.count(awaited));
        node_.notify(error::success, event);
        return events_.await(awaited, expected);
    }

    header_t link(const system::hash_digest& hash) const
    {
        return query_.to_header(hash).value;
    }

    bool candidate(size_t height, const system::hash_digest& hash) const
    {
        return query_.to_candidate(height) == query_.to_header(hash);
    }

    bool confirmed(size_t height, const system::hash_digest& hash) const
    {
        return query_.to_confirmed(height) == query_.to_header(hash);
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

    /// Push an archived header onto the confirmed chain directly.
    bool confirm(const system::hash_digest& hash)
    {
        return query_.push_confirmed(query_.to_header(hash), false);
    }

protected:
    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    harness_node node_;
    header_accessor header_;
    chase_recorder events_{};
};

struct chaser_header_current_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_current_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            config.node.currency_window_minutes = 60;
        })
    {
    }
};

struct chaser_header_unmarked_setup_fixture
  : chaser_header_setup_fixture
{
    chaser_header_unmarked_setup_fixture()
      : chaser_header_setup_fixture([](configuration& config)
        {
            config.database.mark_unconfirmable = false;
        })
    {
    }
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

BC_AUTO_TEST_SUITE(chaser_header_tests)

// validate
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__validate__block1__success, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BC_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BC_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), error::success);
}

BC_FIXTURE_TEST_CASE(chaser_header__validate__unsatisfied_work__invalid_proof_of_work, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BC_REQUIRE(genesis);

    const auto header = make(genesis_hash(), 1);
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BC_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), system::error::invalid_proof_of_work);
}

BC_FIXTURE_TEST_CASE(chaser_header__validate__version_under_bip34_minimum__insufficient_block_version, chaser_header_fork_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BC_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    BC_REQUIRE_EQUAL(chaser_header::validate(*header, state, config_.bitcoin), system::error::insufficient_block_version);
}

BC_FIXTURE_TEST_CASE(chaser_header__validate__checkpoint_hash_mismatch__checkpoint_conflict, chaser_header_setup_fixture)
{
    const auto genesis = query_.get_candidate_chain_state(config_.bitcoin, 0);
    BC_REQUIRE(genesis);

    const auto header = block1();
    const system::chain::chain_state state{ *genesis, *header, config_.bitcoin };
    system::settings bitcoin{ config_.bitcoin };
    bitcoin.checkpoints.emplace_back(system::null_hash, 1);
    BC_REQUIRE_EQUAL(chaser_header::validate(*header, state, bitcoin), system::error::checkpoint_conflict);
}

// organize (unproven)
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__organize__block1__candidate, chaser_header_setup_fixture)
{
    const auto header = block1();
    const auto result = organize(header);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BC_REQUIRE(candidate(1, header->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__organize__archived__duplicate_header, chaser_header_setup_fixture)
{
    BC_REQUIRE_EQUAL(organize(block1()).first, error::success);

    const auto result = organize(block1());
    BC_REQUIRE_EQUAL(result.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(result.second, max_size_t);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize__unsatisfied_work__invalid_proof_of_work, chaser_header_setup_fixture)
{
    const auto result = organize(make(genesis_hash(), 1));
    BC_REQUIRE_EQUAL(result.first, system::error::invalid_proof_of_work);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize__unknown_parent__orphan_header, chaser_header_setup_fixture)
{
    const auto result = organize(make(system::one_hash, 1));
    BC_REQUIRE_EQUAL(result.first, error::orphan_header);
    BC_REQUIRE_EQUAL(result.second, 0u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

// organize (proven)
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__unsatisfied_work__candidate, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto result = organize(a1, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__unconfirmable_parent__block_unconfirmable, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(query_.set_block_unconfirmable(query_.to_header(a1->hash())));

    const auto result = organize(make(a1->hash(), 2), false);
    BC_REQUIRE_EQUAL(result.first, database::error::block_unconfirmable);
    BC_REQUIRE_EQUAL(result.second, 0u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__archived_unconfirmable__block_unconfirmable_height, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(query_.set_block_unconfirmable(query_.to_header(a1->hash())));

    const auto result = organize(make(genesis_hash(), 1), false);
    BC_REQUIRE_EQUAL(result.first, database::error::block_unconfirmable);
    BC_REQUIRE_EQUAL(result.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__equal_work_branch__cached_not_archived, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);

    const auto result = organize(b1, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(query_.to_header(b1->hash()).is_terminal());

    const auto duplicate = organize(make(genesis_hash(), 2), false);
    BC_REQUIRE_EQUAL(duplicate.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(duplicate.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__stronger_cached_branch__reorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    const auto b2 = make(b1->hash(), 3);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE(query_.to_header(b1->hash()).is_terminal());

    const auto result = organize(b2, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 2u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(candidate(1, b1->hash()));
    BC_REQUIRE(candidate(2, b2->hash()));
    BC_REQUIRE(!query_.to_header(a1->hash()).is_terminal());
    BC_REQUIRE(!query_.is_candidate_header(query_.to_header(a1->hash())));

    // The cached branch is archived by the reorganization and not current.
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 0u);
    BC_REQUIRE_EQUAL(events_.count(chase::headers), 0u);
    BC_REQUIRE_EQUAL(events_.count(chase::bump), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__stronger_archived_branch__reorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 4);
    const auto a3 = make(a2->hash(), 5);
    const auto b1 = make(genesis_hash(), 2);
    const auto b2 = make(b1->hash(), 3);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE(candidate(2, b2->hash()));

    // Extends the archived (reorganized out) a1 to a tie, so cached.
    const auto weak = organize(a2, false);
    BC_REQUIRE_EQUAL(weak.first, error::success);
    BC_REQUIRE_EQUAL(weak.second, 2u);
    BC_REQUIRE(candidate(2, b2->hash()));
    BC_REQUIRE(query_.to_header(a2->hash()).is_terminal());

    // Branch is cached a2 over archived a1, both pushed in order.
    const auto result = organize(a3, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 3u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 3u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(candidate(3, a3->hash()));
    BC_REQUIRE(!query_.is_candidate_header(query_.to_header(b1->hash())));
    BC_REQUIRE(!query_.is_candidate_header(query_.to_header(b2->hash())));
    BC_REQUIRE(events_.await(chase::regressed, 2));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>(2)->branch_point, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__deep_cached_branch__reorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto b1 = make(genesis_hash(), 4);
    const auto b2 = make(b1->hash(), 5);
    const auto b3 = make(b2->hash(), 6);
    const auto b4 = make(b3->hash(), 7);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b3, false).first, error::success);
    BC_REQUIRE(candidate(3, a3->hash()));
    BC_REQUIRE(query_.to_header(b3->hash()).is_terminal());

    const auto result = organize(b4, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 4u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 4u);
    BC_REQUIRE(candidate(1, b1->hash()));
    BC_REQUIRE(candidate(2, b2->hash()));
    BC_REQUIRE(candidate(3, b3->hash()));
    BC_REQUIRE(candidate(4, b4->hash()));
    BC_REQUIRE(!query_.is_candidate_header(query_.to_header(a3->hash())));
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__branch_above_genesis__reorganized_at_branch_point, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto c3 = make(a2->hash(), 4);
    const auto c4 = make(c3->hash(), 5);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(c3, false).first, error::success);
    BC_REQUIRE(candidate(3, a3->hash()));

    const auto result = organize(c4, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 4u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(candidate(3, c3->hash()));
    BC_REQUIRE(candidate(4, c4->hash()));
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 2u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__reorganized_twice__both_regressions_notified, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    const auto b2 = make(b1->hash(), 3);
    const auto a2 = make(a1->hash(), 4);
    const auto a3 = make(a2->hash(), 5);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);
    BC_REQUIRE(candidate(3, a3->hash()));
    BC_REQUIRE(events_.await(chase::regressed, 2));
    BC_REQUIRE_EQUAL(events_.count(chase::regressed), 2u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__milestone__archived_milestone, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, true).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(query_.is_milestone(query_.to_header(a1->hash())));
    BC_REQUIRE(!query_.is_milestone(query_.to_header(a2->hash())));
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__current_header__bumped_once_and_headers, chaser_header_current_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1, now());
    const auto a2 = make(a1->hash(), 2, now());
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(events_.await(chase::bump));
    BC_REQUIRE_EQUAL(events_.payload<chase::bump>()->height, 1u);
    BC_REQUIRE(events_.await(chase::headers));
    BC_REQUIRE_EQUAL(events_.payload<chase::headers>()->branch_point, 0u);

    // Only the first current header following a regression bumps.
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(events_.await(chase::headers, 2));
    BC_REQUIRE_EQUAL(events_.payload<chase::headers>(2)->branch_point, 1u);
    BC_REQUIRE_EQUAL(events_.count(chase::bump), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__current_reorganization__bumped_above_branch_point, chaser_header_current_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1, now());
    const auto a2 = make(a1->hash(), 2, now());
    const auto c2 = make(a1->hash(), 3, now());
    const auto c3 = make(c2->hash(), 4, now());
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(c2, false).first, error::success);
    BC_REQUIRE(events_.await(chase::bump));

    BC_REQUIRE_EQUAL(organize(c3, false).first, error::success);
    BC_REQUIRE(candidate(3, c3->hash()));
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 1u);
    BC_REQUIRE(events_.await(chase::bump, 2));
    BC_REQUIRE_EQUAL(events_.payload<chase::bump>(2)->height, 2u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__fork_activation__candidate, chaser_header_fork_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1, config_.bitcoin.bip16_activation_time, config_.bitcoin.bip34_version);
    const auto result = organize(a1, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
}

// organize (store faults)
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__candidate_above_chaser_top__organize4, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto x2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(push(*x2, 2, 3));

    const auto result = organize(make(x2->hash(), 3), false);
    BC_REQUIRE_EQUAL(result.first, error::organize4);
    BC_REQUIRE_EQUAL(result.second, 3u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__candidate_below_chaser_top__organize5, chaser_header_setup_fixture)
{
    BC_REQUIRE_EQUAL(organize(make(genesis_hash(), 1), false).first, error::success);
    BC_REQUIRE(query_.pop_candidate());

    const auto result = organize(make(genesis_hash(), 2), false);
    BC_REQUIRE_EQUAL(result.first, error::organize5);
    BC_REQUIRE_EQUAL(result.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__unstored_candidate_top__organize3, chaser_header_setup_fixture)
{
    BC_REQUIRE_EQUAL(organize(make(genesis_hash(), 1), false).first, error::success);
    BC_REQUIRE(query_.push_candidate(database::header_link{ 42 }));

    const auto result = organize(make(genesis_hash(), 2), false);
    BC_REQUIRE_EQUAL(result.first, error::organize3);
    BC_REQUIRE_EQUAL(result.second, 1u);
}

// checkpoints
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__checkpoint_hash_mismatch__checkpoint_conflict, chaser_header_conflict_setup_fixture)
{
    const auto result = organize(make(genesis_hash(), 1), false);
    BC_REQUIRE_EQUAL(result.first, system::error::checkpoint_conflict);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__under_reached_checkpoint__checkpoint_conflict, chaser_header_checkpoint_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 3);
    const auto b1 = make(genesis_hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);

    const auto purged = organize(make(genesis_hash(), 2), false);
    BC_REQUIRE_EQUAL(purged.first, system::error::checkpoint_conflict);
    BC_REQUIRE_EQUAL(purged.second, 0u);

    const auto orphan = organize(make(system::one_hash, 4), false);
    BC_REQUIRE_EQUAL(orphan.first, error::orphan_header);
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__above_reached_checkpoint__reorganized, chaser_header_checkpoint_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 3);
    const auto a3 = make(a2->hash(), 4);
    const auto c3 = make(a2->hash(), 5);
    const auto c4 = make(c3->hash(), 6);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);

    const auto weak = organize(c3, false);
    BC_REQUIRE_EQUAL(weak.first, error::success);
    BC_REQUIRE_EQUAL(weak.second, 3u);
    BC_REQUIRE(candidate(3, a3->hash()));

    const auto result = organize(c4, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 4u);
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(candidate(3, c3->hash()));
    BC_REQUIRE(candidate(4, c4->hash()));
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 2u);
}

// window
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__branch_under_window__purged, chaser_header_window_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto a4 = make(a3->hash(), 4);
    const auto b1 = make(genesis_hash(), 5);
    const auto b2 = make(b1->hash(), 6);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);

    const auto retained = organize(make(b1->hash(), 6), false);
    BC_REQUIRE_EQUAL(retained.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(retained.second, 2u);

    BC_REQUIRE_EQUAL(organize(a4, false).first, error::success);

    const auto purged = organize(make(genesis_hash(), 5), false);
    BC_REQUIRE_EQUAL(purged.first, error::success);
    BC_REQUIRE_EQUAL(purged.second, 1u);
    BC_REQUIRE(candidate(4, a4->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__organize_proven__sibling_branches_under_window__purged, chaser_header_window_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto a3 = make(a2->hash(), 3);
    const auto a4 = make(a3->hash(), 4);
    const auto b1 = make(genesis_hash(), 5);
    const auto b2 = make(b1->hash(), 6);
    const auto c2 = make(b1->hash(), 7);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(c2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a3, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a4, false).first, error::success);
    BC_REQUIRE(candidate(4, a4->hash()));

    const auto purged = organize(make(genesis_hash(), 5), false);
    BC_REQUIRE_EQUAL(purged.first, error::success);
    BC_REQUIRE_EQUAL(purged.second, 1u);
}

// prioritize
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__prioritize__unknown__not_found, chaser_header_setup_fixture)
{
    const auto result = prioritize(system::one_hash);
    BC_REQUIRE_EQUAL(result.first, database::error::not_found);
    BC_REQUIRE_EQUAL(result.second, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__prioritize__tied_branch__reorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE(candidate(1, a1->hash()));

    const auto result = prioritize(b1->hash());
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE(candidate(1, b1->hash()));
    BC_REQUIRE(!query_.is_candidate_header(query_.to_header(a1->hash())));
    BC_REQUIRE(events_.await(chase::regressed));
    BC_REQUIRE_EQUAL(events_.payload<chase::regressed>()->branch_point, 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__prioritize__weaker_branch__cached_unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b1 = make(genesis_hash(), 3);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);

    const auto result = prioritize(b1->hash());
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(query_.to_header(b1->hash()).is_terminal());
    BC_REQUIRE_EQUAL(events_.count(chase::regressed), 0u);

    // The branch remains cached, so it reorganizes once it has more work.
    const auto b2 = make(b1->hash(), 4);
    const auto b3 = make(b2->hash(), 5);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b3, false).first, error::success);
    BC_REQUIRE(candidate(3, b3->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__prioritize__reorganized_out_tie__archived_duplicate, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto b1 = make(genesis_hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(prioritize(b1->hash()).first, error::success);
    BC_REQUIRE(candidate(1, b1->hash()));

    // The reorganized out a1 is archived, so not cached for prioritization.
    const auto cached = organize(a1, false);
    BC_REQUIRE_EQUAL(cached.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(cached.second, max_size_t);
    BC_REQUIRE(query_.to_header(a1->hash()) != query_.to_candidate(1));
}

BC_FIXTURE_TEST_CASE(chaser_header__prioritize__cached_with_child__unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b1 = make(genesis_hash(), 3);
    const auto b2 = make(b1->hash(), 4);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);

    const auto result = prioritize(b1->hash());
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 0u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
}

// disorganize
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_top__disorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(notify(chases::unvalid{ link(a2->hash()) }, chase::disorganized));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
    BC_REQUIRE(unconfirmable(a2->hash()));
    BC_REQUIRE(!unconfirmable(a1->hash()));
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 0u);
    BC_REQUIRE(events_.await(chase::suspend));

    // The valid portion of the branch is retained in the tree with state.
    const auto cached = organize(make(genesis_hash(), 1), false);
    BC_REQUIRE_EQUAL(cached.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(cached.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_top__cached_branch_reorganizes_when_extended, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b2 = make(a1->hash(), 3);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(notify(chases::unvalid{ link(a2->hash()) }, chase::disorganized));
    flush();
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);

    // The sibling extends cached a1, which is pushed from the tree.
    const auto result = organize(b2, false);
    BC_REQUIRE_EQUAL(result.first, error::success);
    BC_REQUIRE_EQUAL(result.second, 2u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, b2->hash()));
    BC_REQUIRE(unconfirmable(a2->hash()));

    // The unconfirmable header cannot be extended.
    const auto child = organize(make(a2->hash(), 4), false);
    BC_REQUIRE_EQUAL(child.first, database::error::block_unconfirmable);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unchecked_first__disorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(notify(chases::unchecked{ link(a1->hash()) }, chase::disorganized));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
    BC_REQUIRE(unconfirmable(a1->hash()));
    BC_REQUIRE(unconfirmable(a2->hash()));
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 0u);

    // Nothing of the branch is retained, so a1 is a stored duplicate.
    const auto stored = organize(make(genesis_hash(), 1), false);
    BC_REQUIRE_EQUAL(stored.first, database::error::block_unconfirmable);
    BC_REQUIRE_EQUAL(stored.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unconfirmable_top__disorganized, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(notify(chases::unconfirmable{ link(a1->hash()) }, chase::disorganized));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
    BC_REQUIRE(unconfirmable(a1->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_above_confirmed__confirmed_candidates, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b1 = make(genesis_hash(), 3);
    const auto b2 = make(b1->hash(), 4);
    const auto b3 = make(b2->hash(), 5);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(confirm(a1->hash()));
    BC_REQUIRE(confirm(a2->hash()));
    BC_REQUIRE_EQUAL(organize(b1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b3, false).first, error::success);
    BC_REQUIRE(candidate(3, b3->hash()));
    BC_REQUIRE(notify(chases::unvalid{ link(b2->hash()) }, chase::disorganized));
    flush();

    // Candidates are reset to the confirmed chain above the fork point.
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(unconfirmable(b2->hash()));
    BC_REQUIRE(unconfirmable(b3->hash()));
    BC_REQUIRE(!unconfirmable(b1->hash()));
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 2u);

    const auto cached = organize(make(genesis_hash(), 3), false);
    BC_REQUIRE_EQUAL(cached.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(cached.second, 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_above_confirmed_fork__retained_below_fork, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    const auto b2 = make(a1->hash(), 3);
    const auto b3 = make(b2->hash(), 4);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(confirm(a1->hash()));
    BC_REQUIRE(confirm(a2->hash()));
    BC_REQUIRE_EQUAL(organize(b2, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(b3, false).first, error::success);
    BC_REQUIRE(candidate(3, b3->hash()));
    BC_REQUIRE(notify(chases::unvalid{ link(b3->hash()) }, chase::disorganized));
    flush();

    // The fork point (a1) is retained and the confirmed a2 is restored.
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(unconfirmable(b3->hash()));
    BC_REQUIRE(!unconfirmable(b2->hash()));
    BC_REQUIRE(!unconfirmable(a1->hash()));
    BC_REQUIRE_EQUAL(events_.payload<chase::disorganized>()->branch_point, 2u);

    const auto cached = organize(make(a1->hash(), 3), false);
    BC_REQUIRE_EQUAL(cached.first, error::duplicate_header);
    BC_REQUIRE_EQUAL(cached.second, 2u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_twice__second_ignored, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(notify(chases::unvalid{ link(a2->hash()) }, chase::disorganized));
    BC_REQUIRE(notify(chases::unvalid{ link(a1->hash()) }, chase::unvalid));
    flush();

    // The second link is no longer a candidate, so it is not marked.
    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 0u);
    BC_REQUIRE(unconfirmable(a2->hash()));
    BC_REQUIRE(!unconfirmable(a1->hash()));
    BC_REQUIRE_EQUAL(events_.count(chase::disorganized), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_not_candidate__unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto x1 = make(genesis_hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(archive(*x1, 1, 2));
    BC_REQUIRE(notify(chases::unvalid{ link(x1->hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(!unconfirmable(x1->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_fork_point__unchanged, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(notify(chases::unvalid{ link(genesis_hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BC_REQUIRE(candidate(1, a1->hash()));
    BC_REQUIRE(!unconfirmable(genesis_hash()));
    BC_REQUIRE_EQUAL(events_.count(chase::disorganized), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unvalid_unmarked__unchanged, chaser_header_unmarked_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    const auto a2 = make(a1->hash(), 2);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE_EQUAL(organize(a2, false).first, error::success);
    BC_REQUIRE(notify(chases::unvalid{ link(a2->hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(candidate(2, a2->hash()));
    BC_REQUIRE(!unconfirmable(a2->hash()));
    BC_REQUIRE_EQUAL(events_.count(chase::disorganized), 0u);
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__stop__unsubscribed, chaser_header_setup_fixture)
{
    const auto a1 = make(genesis_hash(), 1);
    BC_REQUIRE_EQUAL(organize(a1, false).first, error::success);
    BC_REQUIRE(notify(chases::stop{}, chase::stop));
    BC_REQUIRE(notify(chases::unvalid{ link(a1->hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 1u);
    BC_REQUIRE(!unconfirmable(a1->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unstored_fork_point__unchanged, chaser_header_setup_fixture)
{
    const auto x2 = make(genesis_hash(), 1);
    BC_REQUIRE(query_.push_candidate(database::header_link{ 42 }));
    BC_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
    BC_REQUIRE(push(*x2, 2, 2));
    BC_REQUIRE(notify(chases::unvalid{ link(x2->hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(!unconfirmable(x2->hash()));
}

BC_FIXTURE_TEST_CASE(chaser_header__handle_chase__unstored_candidate_below__unchanged, chaser_header_setup_fixture)
{
    const auto x2 = make(genesis_hash(), 1);
    BC_REQUIRE(query_.push_candidate(database::header_link{ 42 }));
    BC_REQUIRE(push(*x2, 2, 2));
    BC_REQUIRE(notify(chases::unvalid{ link(x2->hash()) }, chase::unvalid));
    flush();

    BC_REQUIRE_EQUAL(query_.get_top_candidate(), 2u);
    BC_REQUIRE(!unconfirmable(x2->hash()));
}

BC_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
