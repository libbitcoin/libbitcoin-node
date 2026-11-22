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
#ifndef LIBBITCOIN_NODE_TEST_FUNCTIONAL_P2P_SETUP_FIXTURE
#define LIBBITCOIN_NODE_TEST_FUNCTIONAL_P2P_SETUP_FIXTURE

#include "../test.hpp"
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>

#define P2P_FUNCTIONAL_ENDPOINT "127.0.0.1"

// A full node that retains its peer acceptor, for its bound endpoint.
class bound_node
  : public full_node
{
public:
    using full_node::full_node;

    /// The endpoint to which the peer acceptor is bound (requires run).
    network::io::endpoint binding() NOEXCEPT
    {
        std::promise<network::io::endpoint> bound{};
        network::io::post(strand(), [&]() NOEXCEPT
        {
            bound.set_value(acceptor_->local().to_endpoint());
        });

        return bound.get_future().get();
    }

protected:
    network::acceptor::ptr create_acceptor(
        const network::socket::context& context) NOEXCEPT override
    {
        acceptor_ = full_node::create_acceptor(context);
        return acceptor_;
    }

private:
    network::acceptor::ptr acceptor_{};
};

// Runs real full node accepting on loopback, test acting as remote peer over
// raw blocking socket (framing via peer messages).
struct p2p_setup_fixture
{
    DELETE_COPY_MOVE(p2p_setup_fixture);

    using initializer = std::function<bool(node::query&)>;
    using configurator = std::function<void(configuration&)>;
    explicit p2p_setup_fixture(const initializer& setup={},
        const configurator& configure={});
    ~p2p_setup_fixture();

    /// Write a framed message to the node.
    void send(const std::string& command, const system::data_chunk& payload);

    /// Serialize and write a framed message to the node.
    template <class Message>
    void send(const Message& message, uint32_t version)
    {
        system::data_chunk payload(message.size(version));
        BC_REQUIRE(message.serialize(version, payload));
        send(Message::command, payload);
    }

    /// Read one framed message from the node.
    std::pair<std::string, system::data_chunk> receive();

    /// Read framed messages from the node until the command matches.
    system::data_chunk receive(const std::string& command);

    /// Read framed messages until the command (true) or the terminal.
    bool received(const std::string& command, const std::string& terminal);

    /// Read framed messages, true if node drops connection before command sent.
    bool dropped(const std::string& command);

    /// Perform the version handshake, retains the node's version message.
    bool handshake(uint64_t services=0,
        uint32_t version=network::messages::peer::level::maximum_protocol,
        bool relay=false, bool witness_tx=false);

    /// The node's version message (set by handshake).
    network::messages::peer::version::cptr node_version{};

    /// Connect another peer to the node, made the active peer.
    size_t connect();

    /// Accept a manual connection from the node, made the active peer.
    size_t accept();

    /// Make the peer at the index the active peer (the first is zero).
    void select(size_t peer);

    /// Close the active peer's connection to the node.
    void disconnect();

    /// Subscribe to chaser events, returning the subscription key.
    object_key subscribe();

    /// Wait (bounded) for the next event of the payload's type.
    template <class Payload>
    bool await(Payload& payload)
    {
        std::unique_lock lock(subscription_->mutex);
        return subscription_->signal.wait_for(lock, std::chrono::seconds(10),
            [&]()
            {
                auto& values = subscription_->values;
                while (!values.empty())
                {
                    const auto value = values.front();
                    values.pop_front();
                    if (to_chase(value) == Payload::id)
                    {
                        payload = std::get<Payload>(value);
                        return true;
                    }
                }

                return false;
            });
    }

    /// The value of the first event of the type fired from the act onward.
    uint64_t fired_value(uint8_t event, const std::function<void()>& act);

protected:
    static configuration configured(const configurator& configure);

    struct subscription
    {
        std::mutex mutex{};
        std::condition_variable signal{};
        std::deque<event_value> values{};
    };

    configuration config_;
    node::store store_;
    node::query query_;
    network::logger log_{};
    bound_node node_;

private:
    network::io::socket& socket();

    network::io::context io_{};
    std::deque<network::io::socket> sockets_{};
    size_t active_{};
    std::shared_ptr<subscription> subscription_
    {
        std::make_shared<subscription>()
    };
};

// A node configured to reply not_found.
struct p2p_not_found_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_not_found_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.network.enable_not_found = true;
        })
    {
    }
};

// A node that relays transactions and replies not_found.
struct p2p_relay_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_relay_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.enable_not_found = true;
        })
    {
    }
};

// A current node that relays transactions by witness hash.
struct p2p_witness_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_witness_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.network.enable_relay = true;
            config.network.enable_not_found = true;
            config.network.enable_witness_tx = true;
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

// A node that does not store the blocks it has pruned.
struct p2p_limited_setup_fixture
  : p2p_setup_fixture
{
    /// Archives mainnet block 1 pruned.
    static bool pruned(node::query& query) NOEXCEPT;

    inline p2p_limited_setup_fixture()
      : p2p_setup_fixture(pruned, [](configuration& config)
        {
            config.node.limited_blocks = true;
            config.network.enable_not_found = true;
        })
    {
    }
};

// Header archived before its block is associated, so header link resolves
// while block remains absent from archive.
// Steady state of headers-first synchronization.
struct p2p_unassociated_setup_fixture
  : p2p_setup_fixture
{
    static system::chain::header unassociated() NOEXCEPT;

    inline p2p_unassociated_setup_fixture()
      : p2p_setup_fixture([](node::query& query)
        {
            return query.set(unassociated(), database::context{}, {}, false);
        }, [](configuration& config)
        {
            config.network.enable_not_found = true;
        })
    {
    }
};

// Node with compact blocks enabled and chain always current. Candidate above
// confirmed top precludes store prune, which suspends network (dropping test
// peer) when chain coalesces.
struct p2p_compact_setup_fixture
  : p2p_setup_fixture
{
    using condition = std::function<bool()>;

    /// Mainnet blocks 1 and 2 (coinbase only).
    static const system::chain::block& block1() NOEXCEPT;
    static const system::chain::block& block2() NOEXCEPT;

    inline p2p_compact_setup_fixture(const initializer& setup={},
        uint16_t compact_timeout=max_uint16, uint8_t missing_percent=100,
        bool relay=true, uint32_t currency_window=0)
      : p2p_setup_fixture(setup, [=](configuration& config)
        {
            using deployment = system::chain::chain_state::deployment;
            config.bitcoin.bip9_bit0_deployment.start = deployment::always_active;
            config.network.enable_relay = relay;
            config.network.enable_compact = true;
            config.node.currency_window_minutes = currency_window;
            config.node.compact_timeout_seconds = compact_timeout;
            config.node.compact_missing_percent = missing_percent;
            config.bitcoin.minimum_work = {};
        })
    {
    }

    /// Wait (bounded) for the condition to be satisfied.
    bool await(const condition& satisfied);

    /// Wait (bounded) for the block to be associated in the store.
    bool associated(const system::hash_digest& hash);
};

// A compact node that fills only a block fully held by the pool.
struct p2p_compact_pooled_setup_fixture
  : p2p_compact_setup_fixture
{
    inline p2p_compact_pooled_setup_fixture()
      : p2p_compact_setup_fixture({}, max_uint16, 0)
    {
    }
};

// A compact node with relay disabled, so its pool is closed.
struct p2p_compact_closed_setup_fixture
  : p2p_compact_setup_fixture
{
    inline p2p_compact_closed_setup_fixture()
      : p2p_compact_setup_fixture({}, max_uint16, 100, false)
    {
    }
};

// A compact node with a chain that is not current.
struct p2p_compact_stale_setup_fixture
  : p2p_compact_setup_fixture
{
    inline p2p_compact_stale_setup_fixture()
      : p2p_compact_setup_fixture({}, max_uint16, 100, true, one)
    {
    }
};

// A compact node with blocks 1 and 2 as unassociated candidates.
struct p2p_compact_candidate_setup_fixture
  : p2p_compact_setup_fixture
{
    static bool candidate(node::query& query) NOEXCEPT;

    inline p2p_compact_candidate_setup_fixture()
      : p2p_compact_setup_fixture(candidate, 0)
    {
    }
};

#endif
