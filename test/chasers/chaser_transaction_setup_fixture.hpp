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
#ifndef LIBBITCOIN_NODE_TEST_CHASERS_CHASER_TRANSACTION_SETUP_FIXTURE
#define LIBBITCOIN_NODE_TEST_CHASERS_CHASER_TRANSACTION_SETUP_FIXTURE

#include "../test.hpp"
#include "../functional/p2p_setup_fixture.hpp"
#include <chrono>
#include <future>
#include <mutex>
#include <thread>

using namespace system;
namespace peer = network::messages::peer;
using type_id = peer::inventory_item::type_id;

constexpr uint64_t parent_value = 100'000;
const hash_digest missing_hash{ 0x42 };

// The flags of the bip9 bit zero deployment.
constexpr auto deployed = chain::flags::bip68_rule | chain::flags::bip112_rule
    | chain::flags::bip113_rule;

// A tx with an anyone-can-spend output.
inline const chain::transaction& parent() NOEXCEPT
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

inline chain::transaction::cptr spend(const hash_digest& hash, uint64_t value,
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

inline chain::transaction::cptr spend(uint64_t value) NOEXCEPT
{
    return spend(parent().hash(false), value);
}

// A spend of the parent to an output of one sigop.
inline chain::transaction::cptr spend_signed(uint64_t value) NOEXCEPT
{
    const chain::point point{ parent().hash(false), 0u };
    const chain::operations ops{ { chain::opcode::checksig } };
    return std::make_shared<const chain::transaction>(chain::transaction
    {
        1u,
        chain::inputs{ { point, chain::script{}, max_uint32 } },
        chain::outputs{ { value, chain::script{ ops } } },
        0u
    });
}

inline chain::transaction::cptr spend_relative(const hash_digest& hash,
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

// A tx with an output of one push, two of which are not storable as a block.
inline const chain::transaction& large_transaction() NOEXCEPT
{
    const chain::point point{ one_hash, 1u };
    const chain::operations push{ { data_chunk(4'194'304), false } };
    static const chain::transaction instance
    {
        1u,
        chain::inputs{ { point, chain::script{}, max_uint32 } },
        chain::outputs{ { 42u, chain::script{ push } } },
        0u
    };

    return instance;
}

inline chain::transactions_cptr package(
    const chain::transaction_cptrs& txs) NOEXCEPT
{
    return std::make_shared<const chain::transaction_cptrs>(txs);
}

inline peer::inventory announcement(size_t count, uint8_t tag) NOEXCEPT
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

// Block 1 of a coinbase and the parent.
inline const chain::block& parent_block() NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const chain::input null{ chain::point{}, chain::script{}, max_uint32 };
    const chain::output out{ 42u, chain::script{} };
    const chain::transaction coinbase
    {
        1u, chain::inputs{ null }, chain::outputs{ out }, 1u
    };

    const auto timestamp = add1(genesis.timestamp());
    const chain::header header
    {
        1u, genesis.hash(), null_hash, timestamp, genesis.bits(), 1u
    };

    static const chain::block instance{ header, { coinbase, parent() } };
    return instance;
}

// The parent confirmed in block 1.
inline bool stored_parent(node::query& query) NOEXCEPT
{
    const auto& block = parent_block();
    const database::context context{ deployed, 1, 0 };
    return query.set(block, context, {}, false, true) &&
        query.push_confirmed(query.to_header(block.hash()), true);
}

// A tx pooled in a pooling store, as if relay was enabled before.
inline bool pooled_spend(node::query& query) NOEXCEPT
{
    database::tx_link link{};
    const auto tx = spend(parent_value - 1'000);
    query.set_pooling();
    return query.set(parent()) && !query.set_code(link, *tx) &&
        query.set_pooled(link, *tx, database::context{ 0, 1, 0 }, 0);
}

// Mainnet block 1 archived as a header without txs.
inline bool stored_header(node::query& query) NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const auto& header1 = p2p_compact_setup_fixture::block1().header();
    const database::context context1{ deployed, 1, genesis.timestamp() };
    const auto work1 = genesis.proof() + header1.proof();
    return query.set(parent()) && query.set(header1, context1, work1, false);
}

// The header of a block 2 above the parent block.
inline const chain::header& child_header() NOEXCEPT
{
    const auto& header1 = parent_block().header();
    static const chain::header instance
    {
        1u, header1.hash(), null_hash, add1(header1.timestamp()),
        header1.bits(), 2u
    };

    return instance;
}

// The parent confirmed in block 1, block 2 archived as a header without txs.
inline bool stored_child_header(node::query& query) NOEXCEPT
{
    const auto& header1 = parent_block().header();
    const database::context context2{ deployed, 2, header1.timestamp() };
    return stored_parent(query) &&
        query.set(child_header(), context2, {}, false);
}

// Block 1 of a coinbase and a spend of the parent.
inline const chain::block& reorganized_block() NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const chain::input null{ chain::point{}, chain::script{}, max_uint32 };
    const chain::transaction coinbase
    {
        1u,
        chain::inputs{ null },
        chain::outputs{ { 42u, chain::script{} } },
        0u
    };
    const chain::header header
    {
        1u, genesis.hash(), null_hash, add1(genesis.timestamp()),
        genesis.bits(), 0u
    };
    static const chain::block instance
    {
        header, { coinbase, *spend(parent_value) }
    };
    return instance;
}

// Block 1 archived and confirmed, then popped from the confirmed chain.
inline bool stored_reorganized(node::query& query) NOEXCEPT
{
    const auto& block = reorganized_block();
    if (!query.set(parent()) ||
        !query.set(block, database::context{ deployed, 1, 0 }, {}, false, true))
        return false;

    return query.push_confirmed(query.to_header(block.hash()), true) &&
        query.pop_confirmed();
}

// As stored_reorganized, block 1 the pool top when popped.
inline bool stored_unapplied(node::query& query) NOEXCEPT
{
    const auto& block = reorganized_block();
    if (!query.set(parent()) ||
        !query.set(block, database::context{ deployed, 1, 0 }, {}, false, true))
        return false;

    query.set_pooling();
    const auto link = query.to_header(block.hash());
    return query.push_confirmed(link, true) && query.set_pool_top(link) &&
        query.pop_confirmed();
}

// A relative spender of the tx of the reorganized block.
inline chain::transaction::cptr unapplied_spender() NOEXCEPT
{
    const auto& txs = *reorganized_block().transactions_ptr();
    return spend_relative(txs.at(1)->hash(false), parent_value - 1'000, 2);
}

// As stored_unapplied, the relative spender pooled before the pop.
inline bool stored_unapplied_relative(node::query& query) NOEXCEPT
{
    database::tx_link link{};
    const auto& block = reorganized_block();
    const database::context context{ deployed, 1, 0 };
    if (!query.set(parent()) ||
        !query.set(block, context, {}, false, true) ||
        query.set_code(link, *unapplied_spender()))
        return false;

    query.set_pooling();
    const auto top = query.to_header(block.hash());
    return query.push_confirmed(top, true) &&
        query.set_pooled(link, *unapplied_spender(), context, 0) &&
        query.set_pool_top(top) && query.pop_confirmed();
}

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A session to observe wire broadcasts.
class wire_session
  : public network::session_manual
{
public:
    using session_manual::session_manual;
};

struct chaser_transaction_setup_fixture
  : p2p_setup_fixture
{
    using result = std::pair<code, size_t>;

    // The pool opens only under bip113, so its deployment is always active.
    inline chaser_transaction_setup_fixture(const configurator& configure={},
        const initializer& setup=stored_parent)
      : p2p_setup_fixture(setup, [=](configuration& config)
        {
            using deployment = chain::chain_state::deployment;
            constexpr auto always = deployment::always_active;
            config.bitcoin.bip9_bit0_deployment.start = always;
            if (configure)
                configure(config);
        })
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

    result submit_announced(const chain::transactions_cptr& txs, size_t slot)
    {
        std::promise<result> promise{};
        const auto handler = [&](const code& ec, size_t index) NOEXCEPT
        {
            promise.set_value({ ec, index });
        };

        node_.submit_announced(txs, slot, handler);
        return promise.get_future().get();
    }

    // Reserve a relay slot of the tier and start relay to it.
    size_t reserve_relaying(node::relay_tier tier=node::relay_tier::manual)
    {
        recording();
        const auto slot = node_.reserve_relay(tier);
        node_.activate_relay(slot, relays_->key);
        return slot;
    }

    // Reserve every free relay slot of the tier, returning the count.
    size_t reserve_relays(node::relay_tier tier=node::relay_tier::manual)
    {
        size_t count{};
        while (node_.reserve_relay(tier) < node::relay_slots)
            ++count;

        return count;
    }

    // Relay events of fixture subscription, zero word marker completes wait.
    struct relays
    {
        std::mutex mutex{};
        std::vector<chases::relay> events{};
        std::shared_ptr<std::promise<void>> marked{};
        object_key key{};
    };

    // Subscribe recorder (once).
    void recording()
    {
        if (relays_)
            return;

        const auto out = std::make_shared<relays>();
        std::promise<void> subscribed{};
        node_.subscribe_chase([out](const code&, event_value value) NOEXCEPT
        {
            if (to_chase(value) != chase::relay)
                return true;

            const auto& relay = to_payload<chase::relay>(value);
            std::unique_lock lock{ out->mutex };
            if (is_nonzero(relay.slots))
            {
                out->events.push_back(relay);
                return true;
            }

            if (out->marked)
                out->marked->set_value();

            out->marked.reset();
            return true;
        }, [&](const code&, object_key key) NOEXCEPT
        {
            out->key = key;
            subscribed.set_value();
        });

        subscribed.get_future().wait();
        relays_ = out;
    }

    // Count of relay events recorded, once those notified have arrived.
    size_t record()
    {
        recording();
        const auto marked = std::make_shared<std::promise<void>>();
        auto arrived = marked->get_future();
        {
            std::unique_lock lock{ relays_->mutex };
            relays_->marked = marked;
        }

        node_.notify(node::error::success, chases::relay{ 0, 0 });
        arrived.wait();
        std::unique_lock lock{ relays_->mutex };
        return relays_->events.size();
    }

    // Relay events since record, as tx hash and slots.
    using relayed = std::vector<std::pair<hash_digest, uint64_t>>;
    relayed recorded(size_t from)
    {
        const auto to = record();
        relayed out{};
        std::unique_lock lock{ relays_->mutex };
        for (auto at = from; at < to; ++at)
        {
            const auto& event = relays_->events.at(at);
            out.emplace_back(query_.get_tx_key(event.link), event.slots);
        }

        return out;
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

    using archive_result = std::pair<bool, code>;
    archive_result archive_block(const database::header_link& link)
    {
        const auto& block1 = p2p_compact_setup_fixture::block1();
        const auto block = std::make_shared<const chain::view::block>(
            block1.to_data(true), true);

        code ec{};
        std::promise<code> promise{};
        const auto local = node_.archive_block(ec, block, link, false, false,
            1, false, [&](const code& result) NOEXCEPT
            {
                promise.set_value(result);
            });

        return { local, local ? ec : promise.get_future().get() };
    }

    using built = std::tuple<code, size_t, database::tx_links>;
    built build_template()
    {
        std::promise<built> promise{};
        node_.build_template([&](const code& ec,
            const system::chain::context& context, const system::hash_digest&,
            const database::tx_links& links) NOEXCEPT
        {
            promise.set_value({ ec, context.height, links });
        });

        return promise.get_future().get();
    }

    using estimated = std::pair<code, uint64_t>;
    estimated estimate(size_t target,
        node::estimate_mode mode=node::estimate_mode::basic)
    {
        std::promise<estimated> promise{};
        node_.estimate(target, mode, [&](const code& ec, uint64_t rate) NOEXCEPT
        {
            promise.set_value({ ec, rate });
        });

        return promise.get_future().get();
    }

    using listed = std::pair<code, database::tx_links>;
    listed get_confirmable_txs()
    {
        std::promise<listed> promise{};
        node_.get_confirmable_txs([&](const code& ec,
            const database::tx_links& links) NOEXCEPT
        {
            promise.set_value({ ec, links });
        });

        return promise.get_future().get();
    }

    using rated = std::pair<code, database::fee_rates>;
    rated get_confirmable_fees()
    {
        std::promise<rated> promise{};
        node_.get_confirmable_fees([&](const code& ec,
            const database::fee_rates& rates) NOEXCEPT
        {
            promise.set_value({ ec, rates });
        });

        return promise.get_future().get();
    }

    // Confirm a block of a coinbase, and the tx if any, and notify of it.
    database::header_link confirm(const chain::transaction::cptr& tx={})
    {
        const auto link = push(tx);
        if (!link.is_terminal())
            node_.notify(node::error::success, chases::organized{ link });

        return link;
    }

    // Confirm a block of a coinbase, and the tx if any, in the store only,
    // timestamped as given, or after genesis by its height if zero.
    database::header_link push(const chain::transaction::cptr& tx={},
        uint32_t timestamp=0)
    {
        const system::settings bitcoin{ chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block;
        const auto top = query_.get_top_confirmed();
        const auto height = add1(top);
        const auto time = is_zero(timestamp) ? genesis.header().timestamp() +
            possible_narrow_cast<uint32_t>(height) : timestamp;
        const auto bits = genesis.header().bits();
        const chain::transaction coinbase
        {
            1u,
            chain::inputs{ { chain::point{}, chain::script{}, max_uint32 } },
            chain::outputs{ { 42u, chain::script{} } },
            0u
        };
        const chain::header header
        {
            1u, query_.get_top_confirmed_hash(), null_hash, time, bits, 0u
        };
        chain::transactions txs{ coinbase };
        if (tx)
            txs.push_back(*tx);

        const chain::block block{ header, txs };
        const auto at = possible_narrow_cast<uint32_t>(height);
        const database::context context{ 0, at, 0 };
        if (!query_.set(block, context, {}, false, true))
            return {};

        const auto link = query_.to_header(block.hash());
        if (!query_.push_candidate(link) || !query_.push_confirmed(link, true))
            return {};

        return link;
    }

    // True once a template build reports the pool closed.
    bool pool_closed()
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (std::get<0>(build_template()) == node::error::pooling_disabled)
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    // True once the pool context is of the height.
    bool bumped(size_t height)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (std::get<1>(build_template()) == height)
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    // True once the estimate is the rate.
    bool estimated_at(size_t target, node::estimate_mode mode, uint64_t rate)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            const auto result = estimate(target, mode);
            if (!result.first && result.second == rate)
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
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

    // True once the pool top is the block.
    bool topped(const database::header_link& link)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (query_.get_pool_top() == link)
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

    // The first count of pool scans reported from the act, empty if fewer.
    using pool_events = std::vector<node::pool_event>;
    pool_events reported(size_t count, const std::function<void()>& act)
    {
        struct state
        {
            std::promise<void> done{};
            pool_events events{};
        };

        // The state is shared, as the subscription may outlive the call.
        const auto shared = std::make_shared<state>();
        auto done = shared->done.get_future();
        node_.subscribe_pool([=](const code& ec,
            const node::pool_event& event) NOEXCEPT
        {
            if (ec)
                return false;

            shared->events.push_back(event);
            if (shared->events.size() < count)
                return true;

            shared->done.set_value();
            return false;
        });

        act();
        using namespace std::chrono;
        if (done.wait_for(seconds(10)) != std::future_status::ready)
            return {};

        return shared->events;
    }

    // The values of the event fired by the act, once strand work has settled.
    std::vector<uint64_t> fired(uint8_t event,
        const std::function<void()>& act)
    {
        struct state
        {
            std::promise<bool> done{};
            std::vector<uint64_t> values{};
        };

        // State shared, as subscription may outlive call.
        // Every unsubscription (sentinel or stop code) completes wait.
        const auto shared = std::make_shared<state>();
        auto done = shared->done.get_future();
        log_.subscribe_events([=](const code& ec, uint8_t event_,
            uint64_t value, const auto&) NOEXCEPT
        {
            if (ec)
            {
                shared->done.set_value(false);
                return false;
            }

            if (event_ == events::unknown)
            {
                shared->done.set_value(true);
                return false;
            }

            if (event_ == event)
                shared->values.push_back(value);

            return true;
        });

        // The template is built on the strand, after work posted by the act.
        act();
        build_template();
        log_.fire(events::unknown);
        BC_REQUIRE(done.get());
        return shared->values;
    }

    std::shared_ptr<relays> relays_{};

    // The pool cell of the tx (absent if it has no row).
    uint32_t cell(const database::tx_link& link)
    {
        const auto row = store_.pool.first(link);
        return row.is_terminal() ? pool::algorithm::absent :
            store_.pool_index.cells.at(row.value);
    }

    // True once the pool cell of the tx is the value.
    bool celled(const database::tx_link& link, uint32_t value)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (cell(link) == value)
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

    // Payload of command if received before pong of next ping, else empty.
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

    // Announce the tx, true if the node requests it, then send it.
    bool relay(const chain::transaction::cptr& tx)
    {
        const auto hash = tx->hash(false);
        const auto version = node_version->value;
        send(peer::inventory{ { { type_id::transaction, hash } } }, version);
        if (receive(peer::get_data::command).empty())
            return false;

        send(peer::transaction{ tx }, version);
        return true;
    }

    // The tx hashes announced to the active peer, to the count.
    hashes announced(size_t count)
    {
        hashes out{};
        const auto version = node_version->value;
        while (out.size() < count)
        {
            const auto payload = receive(peer::inventory::command);
            const auto message = peer::inventory::deserialize(version, payload);
            BC_REQUIRE(message);
            for (const auto& item: message->items)
                out.push_back(item.hash);
        }

        return out;
    }

    // The hash of the tx served to the active peer upon request.
    hash_digest served(const hash_digest& hash)
    {
        const auto version = node_version->value;
        send(peer::get_data{ { { type_id::transaction, hash } } }, version);
        const auto payload = receive(peer::transaction::command);
        const auto message = peer::transaction::deserialize(version, payload);
        BC_REQUIRE(message);
        return message->transaction_ptr->hash(false);
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

struct chaser_transaction_distribution_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_distribution_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.inbound.connections = 2;
            config.node.currency_window_minutes = 0;
        })
    {
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

struct chaser_transaction_requests_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_requests_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.network.maximum_requests = 2;
            config.network.request_timeout_seconds = 0;
        })
    {
    }
};

struct chaser_transaction_request_timeout_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_request_timeout_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.network.request_timeout_seconds = 1;
        })
    {
    }
};

struct chaser_transaction_reserves_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_reserves_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.manual_relay_slots = 2;
            config.node.outbound_relay_slots = 3;
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
            config.node.pool.minimum_fee_rate = 0.0000152587890625;
        })
    {
    }
};

struct chaser_transaction_limit_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_limit_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.maximum_outcomes = 1;
        })
    {
    }
};

struct chaser_transaction_reserve_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_reserve_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_weight = 4'000'000;
        })
    {
    }
};

// The coinbase is declared to count every sigop of a block.
struct chaser_transaction_sigops_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_sigops_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_sigops = 80'000;
        })
    {
    }
};

// A block holds one test tx (61 of its 80 virtual bytes).
struct chaser_transaction_single_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_single_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_weight = 3'999'348;
            config.node.pool.estimate_room = 1;
        })
    {
    }
};

// Block holds one test tx, estimate requires room for two.
struct chaser_transaction_room_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_room_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_weight = 3'999'348;
            config.node.pool.estimate_room = 122;
        })
    {
    }
};

// Estimates of a held projection, never expired by age.
struct chaser_transaction_held_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_held_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_weight = 3'999'348;
            config.node.pool.estimate_room = 1;
            config.node.pool.estimate_cache_seconds = max_uint16;
        })
    {
    }
};

// Estimates of the pool as it is, no projection held.
struct chaser_transaction_live_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_live_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.node.pool.coinbase_weight = 3'999'348;
            config.node.pool.estimate_room = 1;
            config.node.pool.estimate_cache_seconds = 0;
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
            using deployment = chain::chain_state::deployment;
            constexpr auto always = deployment::always_active;
            config.bitcoin.bip9_bit0_deployment.start = always;
        })
    {
    }
};

// The bip113 deployment of mainnet (not active above genesis).
struct chaser_transaction_inactive_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_inactive_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            const system::settings bitcoin{ chain::selection::mainnet };
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.bitcoin.bip9_bit0_deployment = bitcoin.bip9_bit0_deployment;
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

struct chaser_transaction_child_header_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_child_header_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        }, stored_child_header)
    {
    }
};

struct chaser_transaction_closed_header_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_closed_header_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = false;
        }, stored_header)
    {
    }
};

struct chaser_transaction_unapplied_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_unapplied_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        }, stored_unapplied)
    {
    }
};

struct chaser_transaction_unapplied_relative_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_unapplied_relative_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        }, stored_unapplied_relative)
    {
    }
};

// Current while the top is within twenty years of now, so the test chain is
// current until a block of the epoch is confirmed.
struct chaser_transaction_currency_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_currency_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 20u * 365u * 24u * 60u;
        })
    {
    }
};

struct chaser_transaction_relay_off_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_relay_off_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = false;
            config.node.currency_window_minutes = 0;
        }, pooled_spend)
    {
    }
};

struct chaser_transaction_reorganized_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_reorganized_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
        }, stored_reorganized)
    {
    }
};

struct chaser_transaction_request_mempool_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_request_mempool_setup_fixture()
      : chaser_transaction_setup_fixture([](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.manual.request_mempool = true;
            config.node.currency_window_minutes = 0;
        })
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

#endif
