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
#include "p2p_setup_fixture.hpp"
#include <chrono>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(functional_p2p_tests, p2p_setup_fixture)

using namespace network::messages::peer;

static constexpr uint64_t network_node = service::node_network;

static system::chain::header::cptr header1() NOEXCEPT
{
    return p2p_compact_setup_fixture::block1().header_ptr();
}

static system::chain::header::cptr header2() NOEXCEPT
{
    return p2p_compact_setup_fixture::block2().header_ptr();
}

// A node with blocks 1 and 2 as unassociated candidates, not current.
struct p2p_candidate_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_candidate_setup_fixture()
      : p2p_setup_fixture(p2p_compact_candidate_setup_fixture::candidate)
    {
    }
};

// A current node with blocks 1 and 2 as unassociated confirmed headers.
struct p2p_confirmed_setup_fixture
  : p2p_compact_setup_fixture
{
    static bool confirmed(node::query& query) NOEXCEPT
    {
        return p2p_compact_candidate_setup_fixture::candidate(query) && query.push_confirmed(query.to_header(block1().hash()), false) && query.push_confirmed(query.to_header(block2().hash()), false);
    }

    inline p2p_confirmed_setup_fixture()
      : p2p_compact_setup_fixture(confirmed)
    {
    }
};

// A node with a checkpoint at block 2 and a milestone at block 1.
struct p2p_checkpoint_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_checkpoint_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.bitcoin.checkpoints = { { header2()->hash(), 2 } };
            config.bitcoin.milestone = { header1()->hash(), 1 };
        })
    {
    }
};

// A current compact node with block 1 archived as unconfirmable.
struct p2p_unconfirmable_setup_fixture
  : p2p_compact_setup_fixture
{
    static bool unconfirmable(node::query& query) NOEXCEPT
    {
        const auto& header = *header1();
        const system::settings bitcoin{ system::chain::selection::mainnet };
        const auto& genesis = bitcoin.genesis_block.header();
        const database::context context{ 0, 1, genesis.timestamp() };
        return query.set(header, context, genesis.proof() + header.proof(), false) && query.set_block_unconfirmable(query.to_header(header.hash()));
    }

    inline p2p_unconfirmable_setup_fixture()
      : p2p_compact_setup_fixture(unconfirmable)
    {
    }
};

static const system::settings& regtest() NOEXCEPT
{
    static const system::settings instance{ system::chain::selection::regtest };
    return instance;
}

// Mine a regtest header (minimal proof of work) extending previous.
static system::chain::header::cptr mine(const system::hash_digest& previous, uint32_t timestamp)
{
    const auto& bitcoin = regtest();
    for (uint32_t nonce{};; ++nonce)
    {
        const auto header = std::make_shared<const system::chain::header>(4u, previous, system::null_hash, timestamp, bitcoin.proof_of_work_limit, nonce);
        if (!header->check(bitcoin.timestamp_limit_seconds, bitcoin.proof_of_work_limit, false))
            return header;
    }
}

// A regtest chain of 4001 headers above genesis (index is height less one).
static const system::chain::header_cptrs& chain()
{
    static const auto instance = []()
    {
        const auto& genesis = regtest().genesis_block.header();
        system::chain::header_cptrs out{};
        auto previous = genesis.hash();
        auto timestamp = genesis.timestamp();
        while (out.size() < 4001u)
        {
            out.push_back(mine(previous, ++timestamp));
            previous = out.back()->hash();
        }

        return out;
    }();

    return instance;
}

// Headers message of the chain headers at heights [first, first + count).
static headers segment(size_t first, size_t count)
{
    const auto begin = std::next(chain().begin(), first - 1u);
    return { { begin, std::next(begin, count) } };
}

// A current regtest node, the sample budget configured by expected headers.
struct p2p_regtest_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_regtest_setup_fixture(uint64_t expected=0)
      : p2p_setup_fixture({}, [=](configuration& config)
        {
            config.bitcoin = regtest();
            config.node.currency_window_minutes = 0;
            config.database.header.expected = expected;
        })
    {
    }

    /// Wait (bounded) for the candidate chain to reach height.
    bool candidate(size_t height)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (query_.get_top_candidate() == height)
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }
};

// A current regtest node with a sample budget of two.
struct p2p_regtest_sampled_setup_fixture
  : p2p_regtest_setup_fixture
{
    inline p2p_regtest_sampled_setup_fixture()
      : p2p_regtest_setup_fixture(4000)
    {
    }
};

// A node with compact blocks enabled and blocks 1 and 2 as unassociated
// candidates, not current.
struct p2p_compact_not_current_setup_fixture
  : p2p_setup_fixture
{
    inline p2p_compact_not_current_setup_fixture()
      : p2p_setup_fixture(p2p_compact_candidate_setup_fixture::candidate, [](configuration& config)
        {
            config.network.enable_compact = true;
        })
    {
    }
};

BOOST_AUTO_TEST_CASE(functional_p2p__handshake__default__provides_network_and_witness)
{
    BOOST_REQUIRE(handshake());
    BOOST_REQUIRE_EQUAL(node_version->value, config_.network.protocol_maximum);
    BOOST_REQUIRE_EQUAL(node_version->services, service::node_network | service::node_witness);
}

BOOST_AUTO_TEST_CASE(functional_p2p__ping__nonce__pong_echo)
{
    BOOST_REQUIRE(handshake());

    constexpr uint64_t expected = 42;
    send(ping{ expected }, node_version->value);

    const auto payload = receive(pong::command);
    const auto message = pong::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, expected);
}

// The block send regression (github.com/libbitcoin/libbitcoin-network/862).
BOOST_AUTO_TEST_CASE(functional_p2p__get_data__genesis_block__expected_bytes)
{
    BOOST_REQUIRE(handshake());

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto expected = genesis.to_data(true);

    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, node_version->value);

    const auto payload = receive(block::command);
    BOOST_REQUIRE_EQUAL(payload.size(), expected.size());
    BOOST_REQUIRE(payload == expected);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_block__not_found,
    p2p_not_found_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const get_data get{ { { inventory_item::type_id::block, system::one_hash } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__pruned_block__not_found,
    p2p_limited_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get{ { { inventory_item::type_id::block, genesis.hash() } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unassociated_block__not_found,
    p2p_unassociated_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const auto hash = unassociated().hash();
    const get_data get{ { { inventory_item::type_id::block, hash } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == hash);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_transaction__not_found,
    p2p_relay_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::maximum_protocol, true));

    const get_data get{ { { inventory_item::type_id::transaction, system::one_hash } } };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

// Below bip130 the 70001 protocol is attached directly (not as 70012 base).
BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_block_70001__not_found,
    p2p_not_found_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::bip61));

    const get_data get{ { { inventory_item::type_id::block, system::one_hash } } };
    send(get, level::bip61);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(level::bip61, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
}

// A run of unservable items is answered by one message.
BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_blocks__one_not_found,
    p2p_not_found_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const get_data get
    {
        {
            { inventory_item::type_id::block, system::one_hash },
            { inventory_item::type_id::block, system::null_hash }
        }
    };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), two);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);
    BOOST_REQUIRE(message->items.back().hash == system::null_hash);
}

// The unservable run is flushed before the send loop resumes.
BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_then_genesis__not_found_then_block,
    p2p_not_found_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get
    {
        {
            { inventory_item::type_id::block, system::one_hash },
            { inventory_item::type_id::block, genesis.hash() }
        }
    };
    send(get, node_version->value);

    const auto payload = receive(not_found::command);
    const auto message = not_found::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == system::one_hash);

    // The send loop resumes and serves the item that follows the run.
    BOOST_REQUIRE_EQUAL(receive(block::command).size(), genesis.to_data(true).size());
}

// not_found is undefined below bip37, so the channel is stopped instead.
BOOST_FIXTURE_TEST_CASE(functional_p2p__get_data__unknown_block_106__stopped,
    p2p_not_found_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::bip35));

    const get_data get{ { { inventory_item::type_id::block, system::one_hash } } };
    send(get, level::bip35);

    // The channel is stopped, so the socket closes without a not_found.
    BOOST_REQUIRE_THROW(receive(not_found::command), boost::system::system_error);
}

// The option is off by default, so the channel is stopped instead.
BOOST_AUTO_TEST_CASE(functional_p2p__get_data__unknown_block_disabled__stopped)
{
    BOOST_REQUIRE(handshake());

    const get_data get{ { { inventory_item::type_id::block, system::one_hash } } };
    send(get, node_version->value);

    BOOST_REQUIRE_THROW(receive(not_found::command), boost::system::system_error);
}

// getheaders (in)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p__get_headers__not_current__empty)
{
    BOOST_REQUIRE(handshake(0, level::headers_protocol));

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_headers{ { genesis.hash() }, system::null_hash }, level::headers_protocol);

    const auto message = headers::deserialize(level::headers_protocol, receive(headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->header_ptrs.empty());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_headers__genesis_locator__confirmed_headers, p2p_confirmed_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::headers_protocol));

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_headers{ { genesis.hash() }, system::null_hash }, level::headers_protocol);

    const auto message = headers::deserialize(level::headers_protocol, receive(headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->header_ptrs.size(), two);
    BOOST_REQUIRE(message->header_ptrs.front()->hash() == header1()->hash());
    BOOST_REQUIRE(message->header_ptrs.back()->hash() == header2()->hash());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__get_headers__top_locator__empty, p2p_confirmed_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::headers_protocol));

    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);

    const auto message = headers::deserialize(level::headers_protocol, receive(headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->header_ptrs.empty());
}

// headers (in, 31800)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(functional_p2p__headers_31800__handshake__get_headers_genesis)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto message = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->start_hashes.size(), one);
    BOOST_REQUIRE(message->start_hashes.front() == genesis.hash());
}

BOOST_AUTO_TEST_CASE(functional_p2p__headers_31800__unknown_parent__get_headers_again)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header2() } }, level::headers_protocol);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto message = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->start_hashes.front() == genesis.hash());
}

BOOST_AUTO_TEST_CASE(functional_p2p__headers_31800__disconnected__stopped)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header1(), header1() } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(functional_p2p__headers_31800__invalid_proof_of_work__stopped)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    const auto& header = *header1();
    const auto invalid = std::make_shared<const system::chain::header>(header.version(), header.previous_block_hash(), header.merkle_root(), header.timestamp(), header.bits(), uint32_t{});
    send(headers{ { invalid } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(functional_p2p__headers_31800__not_current__not_proven)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header1(), header2() } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);
    receive(headers::command);

    BOOST_REQUIRE(query_.to_header(header1()->hash()).is_terminal());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__current__proven_and_archived, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    const headers message{ { header1(), header2() } };
    send(message, level::headers_protocol);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto proven = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(proven);
    BOOST_REQUIRE(proven->start_hashes.front() == genesis.hash());

    send(message, level::headers_protocol);

    const auto archived = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(archived);
    BOOST_REQUIRE(archived->start_hashes.front() == header2()->hash());
    BOOST_REQUIRE(await([&]() { return query_.get_top_candidate() == two; }));
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__archival_orphan__stopped, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header1(), header2() } }, level::headers_protocol);
    receive(get_headers::command);

    send(headers{ { header2() } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__archival_unsampled__stopped, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header1(), header2() } }, level::headers_protocol);
    receive(get_headers::command);

    const auto& header = *header2();
    const auto unsampled = std::make_shared<const system::chain::header>(header.version(), header1()->hash(), system::null_hash, header.timestamp(), header.bits(), header.nonce());
    send(headers{ { header1(), unsampled } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__archival_incomplete__discarded, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(headers{ { header1(), header2() } }, level::headers_protocol);
    receive(get_headers::command);

    send(headers{ { header1() } }, level::headers_protocol);
    send(get_headers{ { header2()->hash() }, system::null_hash }, level::headers_protocol);
    receive(headers::command);

    BOOST_REQUIRE(query_.to_header(header1()->hash()).is_terminal());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__checkpoint__proven_and_archived, p2p_checkpoint_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    const headers message{ { header1(), header2() } };
    send(message, level::headers_protocol);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    const auto proven = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(proven);
    BOOST_REQUIRE(proven->start_hashes.front() == genesis.hash());

    send(message, level::headers_protocol);

    const auto archived = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(archived);
    BOOST_REQUIRE(archived->start_hashes.front() == header2()->hash());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__archival_unconfirmable__stopped, p2p_unconfirmable_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    const headers message{ { header1(), header2() } };
    send(message, level::headers_protocol);
    receive(get_headers::command);

    send(message, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__archival_unconfirmable_parent__stopped, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    const headers message{ { header1() } };
    send(message, level::headers_protocol);
    receive(get_headers::command);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    BOOST_REQUIRE(query_.set_block_unconfirmable(query_.to_header(genesis.hash())));
    send(message, level::headers_protocol);

    BOOST_REQUIRE_THROW(receive(headers::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__full_messages__proven_and_archived, p2p_regtest_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(segment(1, 2000), level::headers_protocol);
    const auto next = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(next);
    BOOST_REQUIRE(next->start_hashes.front() == chain().at(1999)->hash());

    send(segment(2001, 1), level::headers_protocol);
    const auto proven = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(proven);
    BOOST_REQUIRE(proven->start_hashes.front() == config_.bitcoin.genesis_block.hash());

    send(segment(1, 2000), level::headers_protocol);
    const auto collected = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(collected);
    BOOST_REQUIRE(collected->start_hashes.front() == chain().at(1999)->hash());

    send(segment(2001, 1), level::headers_protocol);
    const auto archived = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(archived);
    BOOST_REQUIRE(archived->start_hashes.front() == chain().at(2000)->hash());
    BOOST_REQUIRE(candidate(2001));
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__headers_31800__sample_budget__proven_and_archived, p2p_regtest_sampled_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);

    send(segment(1, 2000), level::headers_protocol);
    receive(get_headers::command);
    send(segment(2001, 2000), level::headers_protocol);
    receive(get_headers::command);
    send(segment(4001, 1), level::headers_protocol);
    const auto proven = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(proven);
    BOOST_REQUIRE(proven->start_hashes.front() == config_.bitcoin.genesis_block.hash());

    send(segment(1, 2000), level::headers_protocol);
    receive(get_headers::command);
    send(segment(2001, 2000), level::headers_protocol);
    receive(get_headers::command);
    send(segment(4001, 1), level::headers_protocol);
    const auto archived = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(archived);
    BOOST_REQUIRE(archived->start_hashes.front() == chain().at(4000)->hash());
    BOOST_REQUIRE(candidate(4001));
}

// inv (in, 31800)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(functional_p2p__inventory_31800__unknown_block__get_headers, p2p_compact_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::headers_protocol));
    receive(get_headers::command);
    send(headers{}, level::headers_protocol);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    send(inventory{ { { inventory_item::type_id::block, genesis.hash() } } }, level::headers_protocol);
    send(inventory{ { { inventory_item::type_id::block, genesis.hash() }, { inventory_item::type_id::block, header1()->hash() } } }, level::headers_protocol);

    const auto request = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(request);
    BOOST_REQUIRE(request->start_hashes.front() == genesis.hash());

    send(headers{ { header1() } }, level::headers_protocol);

    const auto proven = get_headers::deserialize(level::headers_protocol, receive(get_headers::command));
    BOOST_REQUIRE(proven);
    BOOST_REQUIRE(proven->start_hashes.front() == genesis.hash());
}

// sendheaders (in, 70012)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(functional_p2p__send_headers__block_event__headers_announced, p2p_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::bip130));

    send(send_headers{}, level::bip130);
    send(ping{ 42 }, level::bip130);
    receive(pong::command);

    node_.notify({}, node::chases::block{ node::header_t{ database::header_link::terminal } });
    node_.notify({}, node::chases::block{ node::header_t{ query_.to_header(header1()->hash()).value } });

    const auto message = headers::deserialize(level::bip130, receive(headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->header_ptrs.size(), one);
    BOOST_REQUIRE(message->header_ptrs.front()->hash() == header1()->hash());
}

BOOST_FIXTURE_TEST_CASE(functional_p2p__send_headers__announced_by_peer__suppressed, p2p_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node, level::bip130));
    receive(get_headers::command);
    send(headers{}, level::bip130);
    send(headers{ { header1() } }, level::bip130);

    send(send_headers{}, level::bip130);
    send(ping{ 42 }, level::bip130);
    receive(pong::command);

    const system::chain::block& genesis = config_.bitcoin.genesis_block;
    node_.notify({}, node::chases::block{ node::header_t{ query_.to_header(header1()->hash()).value } });
    node_.notify({}, node::chases::block{ node::header_t{ query_.to_header(genesis.hash()).value } });

    const auto message = headers::deserialize(level::bip130, receive(headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->header_ptrs.size(), one);
    BOOST_REQUIRE(message->header_ptrs.front()->hash() == genesis.hash());
}

// sendcmpct (in, 70014)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(functional_p2p__send_compact__unknown_block_event__not_stopped, p2p_compact_not_current_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    send(send_compact{ true, send_compact::compact_version_2 }, node_version->value);
    send(send_headers{}, node_version->value);
    send(ping{ 42 }, node_version->value);
    receive(pong::command);

    node_.notify({}, node::chases::block{ node::header_t{ database::header_link::terminal } });

    constexpr uint64_t expected = 43;
    send(ping{ expected }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, expected);
}

// cmpctblock/blocktxn (in, 70014)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(functional_p2p__compact_transactions__unrequested__ignored, p2p_compact_not_current_setup_fixture)
{
    BOOST_REQUIRE(handshake(network_node));

    const auto& block = p2p_compact_setup_fixture::block1();
    send(compact_transactions{ block.hash(), { block.transactions_ptr()->front() } }, node_version->value);

    constexpr uint64_t expected = 42;
    send(ping{ expected }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, expected);
}

BOOST_AUTO_TEST_SUITE_END()
