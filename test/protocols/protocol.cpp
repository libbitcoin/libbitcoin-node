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
#include <future>

using namespace system;
using namespace network::messages::peer;

static const system::settings mainnet{ chain::selection::mainnet };
static constexpr uint8_t unknown_filter_type = 1;

static chain::header descendant(uint32_t timestamp)
{
    return { 1u, mainnet.genesis_block.hash(), null_hash, timestamp, 0u, 0u };
}

// Unfiltered children of genesis, each archived at the height of its timestamp.
static bool unfiltered(node::query& query)
{
    return query.set(descendant(1), database::context{ 0, 1, 0 }, {}, false) &&
        query.set(descendant(999), database::context{ 0, 999, 0 }, {}, false) &&
        query.set(descendant(1000), database::context{ 0, 1000, 0 }, {}, false) &&
        query.set(descendant(1999), database::context{ 0, 1999, 0 }, {}, false) &&
        query.set(descendant(2000), database::context{ 0, 2000, 0 }, {}, false);
}

// Block 1 confirmed, with block 2 a candidate header above it.
static bool confirmed(node::query& query)
{
    const auto& genesis = mainnet.genesis_block.header();
    const auto& header1 = p2p_compact_setup_fixture::block1().header();
    const auto& header2 = p2p_compact_setup_fixture::block2().header();
    const auto work1 = genesis.proof() + header1.proof();
    const auto work2 = work1 + header2.proof();
    const database::context context1{ 0, 1, genesis.timestamp() };
    const database::context context2{ 0, 2, header1.timestamp() };
    return query.set(p2p_compact_setup_fixture::block1(), context1, work1, false, true) &&
        query.push_candidate(query.to_header(header1.hash())) &&
        query.push_confirmed(query.to_header(header1.hash()), true) &&
        query.set(header2, context2, work2, false) &&
        query.push_candidate(query.to_header(header2.hash()));
}

// The block 1 header with the block 1 coinbase and a spend of it.
static const chain::block& paired()
{
    const auto& coinbase = *p2p_compact_setup_fixture::block1().transactions_ptr()->front();
    static const chain::block instance
    {
        p2p_compact_setup_fixture::block1().header(),
        chain::transactions
        {
            coinbase,
            chain::transaction
            {
                1u,
                chain::inputs{ chain::input{ chain::point{ coinbase.hash(false), 0u }, chain::script{}, max_uint32 } },
                chain::outputs{ chain::output{ 0u, chain::script{} } },
                0u
            }
        }
    };

    return instance;
}

static bool archive(node::query& query, const chain::block& block)
{
    const auto& genesis = mainnet.genesis_block.header();
    const database::context context{ 0, 1, genesis.timestamp() };
    return query.set(block, context, genesis.proof() + block.header().proof(), false, false);
}

struct protocol_paired_setup_fixture
  : p2p_compact_setup_fixture
{
    inline protocol_paired_setup_fixture()
      : p2p_compact_setup_fixture([](node::query& query)
        {
            return archive(query, paired());
        })
    {
    }
};

struct protocol_filter_setup_fixture
  : p2p_setup_fixture
{
    inline protocol_filter_setup_fixture()
      : p2p_setup_fixture(unfiltered, [](configuration& config)
        {
            config.node.provide_filters = true;
            config.database.filter_bk.buckets = 128;
            config.database.filter_tx.buckets = 128;
        })
    {
    }
};

struct protocol_unwitnessed_setup_fixture
  : p2p_setup_fixture
{
    inline protocol_unwitnessed_setup_fixture()
      : p2p_setup_fixture({}, [](configuration& config)
        {
            config.node.provide_witness = false;
        })
    {
    }
};

struct protocol_confirmed_setup_fixture
  : p2p_compact_setup_fixture
{
    inline protocol_confirmed_setup_fixture()
      : p2p_compact_setup_fixture(confirmed)
    {
    }
};

class protocol_session
  : public node::session
{
public:
    explicit protocol_session(full_node& node)
      : node::session(node)
    {
    }
};

class protocol_accessor
  : public node::protocol
{
public:
    explicit protocol_accessor(const node::session::ptr& session)
      : node::protocol(session, {})
    {
    }

    using node::protocol::node_config;
    using node::protocol::database_settings;
    using node::protocol::node_settings;
    using node::protocol::is_current_time;
    using node::protocol::start_time;
    using node::protocol::channel_count;
    using node::protocol::inbound_channel_count;
    using node::protocol::address_count;
    using node::protocol::suspended;
    using node::protocol::suspend;
    using node::protocol::resume;
    using node::protocol::prioritize;
    using node::protocol::organize;
    using node::protocol::estimate;
};

struct protocol_setup_fixture
  : p2p_setup_fixture
{
    protocol_accessor accessor{ std::make_shared<protocol_session>(node_) };
};

// protocol
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_tests, protocol_setup_fixture)

BOOST_AUTO_TEST_CASE(protocol__settings__configured__expected)
{
    BOOST_REQUIRE_EQUAL(accessor.node_config().network.inbound.connections, 1u);
    BOOST_REQUIRE(accessor.database_settings().path == TEST_DIRECTORY);
    BOOST_REQUIRE(!accessor.node_settings().delay_inbound);
}

BOOST_AUTO_TEST_CASE(protocol__is_current_time__genesis__false)
{
    BOOST_REQUIRE(!accessor.is_current_time(mainnet.genesis_block.header().timestamp()));
}

BOOST_AUTO_TEST_CASE(protocol__is_current_time__maximum__true)
{
    BOOST_REQUIRE(accessor.is_current_time(max_uint32));
}

BOOST_AUTO_TEST_CASE(protocol__start_time__started__not_zero)
{
    BOOST_REQUIRE(!is_zero(accessor.start_time()));
}

BOOST_AUTO_TEST_CASE(protocol__channel_count__one_inbound__one)
{
    BOOST_REQUIRE(handshake());
    send(ping{ 42 }, node_version->value);
    receive(pong::command);

    BOOST_REQUIRE_EQUAL(accessor.channel_count(), one);
    BOOST_REQUIRE_EQUAL(accessor.inbound_channel_count(), one);
}

BOOST_AUTO_TEST_CASE(protocol__address_count__empty__zero)
{
    BOOST_REQUIRE_EQUAL(accessor.address_count(), zero);
}

BOOST_AUTO_TEST_CASE(protocol__suspend__resume__not_suspended)
{
    accessor.suspend(node::error::suspended_channel);
    BOOST_REQUIRE(accessor.suspended());
    BOOST_REQUIRE(accessor.resume());
    BOOST_REQUIRE(!accessor.suspended());
}

BOOST_AUTO_TEST_CASE(protocol__prioritize__unknown__not_found)
{
    std::promise<code> result{};
    accessor.prioritize(one_hash, [&](const code& ec, size_t) NOEXCEPT
    {
        result.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(result.get_future().get(), database::error::not_found);
}

BOOST_AUTO_TEST_CASE(protocol__organize__block1_header__height_one)
{
    BOOST_REQUIRE(handshake(0, level::bip35));

    std::promise<std::pair<code, size_t>> result{};
    accessor.organize(p2p_compact_setup_fixture::block1().header_ptr(), [&](const code& ec, size_t height) NOEXCEPT
    {
        result.set_value({ ec, height });
    });

    const auto organized = result.get_future().get();
    BOOST_REQUIRE_EQUAL(organized.first, node::error::success);
    BOOST_REQUIRE_EQUAL(organized.second, one);
}

BOOST_AUTO_TEST_CASE(protocol__organize__block1_header_not_milestone__height_one)
{
    std::promise<std::pair<code, size_t>> result{};
    accessor.organize(p2p_compact_setup_fixture::block1().header_ptr(), false, [&](const code& ec, size_t height) NOEXCEPT
    {
        result.set_value({ ec, height });
    });

    const auto organized = result.get_future().get();
    BOOST_REQUIRE_EQUAL(organized.first, node::error::success);
    BOOST_REQUIRE_EQUAL(organized.second, one);
}

BOOST_AUTO_TEST_CASE(protocol__estimate__default__disabled)
{
    std::promise<code> result{};
    accessor.estimate(1, estimator::mode::basic, [&](const code& ec, uint64_t) NOEXCEPT
    {
        result.set_value(ec);
    });

    BOOST_REQUIRE_EQUAL(result.get_future().get(), node::error::estimate_disabled);
}

BOOST_AUTO_TEST_SUITE_END()

// protocol_observer
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_observer_tests, p2p_setup_fixture)

BOOST_AUTO_TEST_CASE(protocol_observer__inventory__transaction_relay_disallowed__stopped)
{
    BOOST_REQUIRE(handshake());

    send(inventory{ { { inventory_item::type_id::transaction, one_hash } } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_observer__inventory__block_relay_disallowed__pong)
{
    BOOST_REQUIRE(handshake());

    send(inventory{ { { inventory_item::type_id::block, one_hash } } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_SUITE_END()

// protocol_block_out_106
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_block_out_106_tests, p2p_setup_fixture)

BOOST_AUTO_TEST_CASE(protocol_block_out_106__get_data__genesis_106__block)
{
    BOOST_REQUIRE(handshake(0, level::bip35));

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_data{ { { inventory_item::type_id::block, genesis.hash() } } }, level::bip35);

    BOOST_REQUIRE(receive(block::command) == genesis.to_data(true));
}

BOOST_AUTO_TEST_CASE(protocol_block_out_106__get_data__transaction_then_genesis__block)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    const get_data get
    {
        {
            { inventory_item::type_id::transaction, one_hash },
            { inventory_item::type_id::block, genesis.hash() }
        }
    };
    send(get, node_version->value);

    BOOST_REQUIRE(receive(block::command) == genesis.to_data(true));
}

BOOST_AUTO_TEST_CASE(protocol_block_out_106__get_blocks__genesis_106__empty_inventory)
{
    BOOST_REQUIRE(handshake(0, level::bip35));

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_blocks{ { genesis.hash() }, null_hash }, level::bip35);

    const auto message = inventory::deserialize(level::bip35, receive(inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->items.empty());
}

BOOST_FIXTURE_TEST_CASE(protocol_block_out_106__get_blocks__genesis_confirmed_block1__block1, protocol_confirmed_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_blocks{ { genesis.hash() }, null_hash }, node_version->value);

    const auto message = inventory::deserialize(node_version->value, receive(inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == inventory_item::type_id::block);
    BOOST_REQUIRE(message->items.front().hash == block1().hash());
}

BOOST_FIXTURE_TEST_CASE(protocol_block_out_106__get_data__witness_unwitnessed__stopped, protocol_unwitnessed_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_data{ { { inventory_item::type_id::witness_block, genesis.hash() } } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    BOOST_REQUIRE_THROW(receive(pong::command), boost::system::system_error);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_out_106__handle_chase__unknown_then_genesis__genesis_inventory, p2p_compact_candidate_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, level::bip35));
    send(ping{ 42 }, level::bip35);
    receive(pong::command);

    node_.notify({}, node::chases::block{ node::header_t{ 42 } });
    node_.notify({}, node::chases::block{ node::header_t{ 0 } });

    const auto message = inventory::deserialize(level::bip35, receive(inventory::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == inventory_item::type_id::block);
    BOOST_REQUIRE(message->items.front().hash == config_.bitcoin.genesis_block.hash());
}

BOOST_AUTO_TEST_SUITE_END()

// protocol_block_out_70012
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_block_out_70012_tests, p2p_setup_fixture)

BOOST_AUTO_TEST_CASE(protocol_block_out_70012__send_headers__default__pong)
{
    BOOST_REQUIRE(handshake());

    send(send_headers{}, node_version->value);
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_SUITE_END()

// protocol_block_out_70014
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_block_out_70014_tests, p2p_compact_setup_fixture)

BOOST_AUTO_TEST_CASE(protocol_block_out_70014__get_data__genesis_block__block)
{
    BOOST_REQUIRE(handshake());

    const chain::block& genesis = config_.bitcoin.genesis_block;
    send(get_data{ { { inventory_item::type_id::block, genesis.hash() } } }, node_version->value);

    BOOST_REQUIRE(receive(block::command) == genesis.to_data(true));
}

BOOST_AUTO_TEST_CASE(protocol_block_out_70014__get_data__unknown_compact__ignored)
{
    BOOST_REQUIRE(handshake());

    send(get_data{ { { inventory_item::type_id::compact, one_hash } } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_AUTO_TEST_CASE(protocol_block_out_70014__get_compact_transactions__unknown_block__ignored)
{
    BOOST_REQUIRE(handshake());

    send(get_compact_transactions{ one_hash, { 0 } }, node_version->value);
    send(ping{ 42 }, node_version->value);

    const auto message = pong::deserialize(node_version->value, receive(pong::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->nonce, 42u);
}

BOOST_FIXTURE_TEST_CASE(protocol_block_out_70014__get_compact_transactions__second__expected, protocol_paired_setup_fixture)
{
    BOOST_REQUIRE(handshake());

    const auto& block = paired();
    send(get_compact_transactions{ block.hash(), { 1 } }, node_version->value);

    const auto message = compact_transactions::deserialize(node_version->value, receive(compact_transactions::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE(message->block_hash == block.hash());
    BOOST_REQUIRE_EQUAL(message->transaction_ptrs.size(), one);
    BOOST_REQUIRE(message->transaction_ptrs.front()->hash(false) == block.transactions_ptr()->back()->hash(false));
}

BOOST_AUTO_TEST_SUITE_END()

// protocol_filter_out_70015
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(protocol_filter_out_70015_tests, protocol_filter_setup_fixture)

// getcfcheckpt

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_checkpoint__genesis__empty)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filter_checkpoint{ client_filter::type_id::neutrino, hash }, node_version->value);

    const auto message = client_filter_checkpoint::deserialize(node_version->value, receive(client_filter_checkpoint::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->filter_type, client_filter::type_id::neutrino);
    BOOST_REQUIRE(message->stop_hash == hash);
    BOOST_REQUIRE(message->filter_headers.empty());
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_checkpoint__unknown_type__stopped)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filter_checkpoint{ unknown_filter_type, hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_checkpoint::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_checkpoint__unknown_stop_hash__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filter_checkpoint{ client_filter::type_id::neutrino, one_hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_checkpoint::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_checkpoint__unconfirmed_interval__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filter_checkpoint{ client_filter::type_id::neutrino, descendant(1000).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_checkpoint::command), boost::system::system_error);
}

// getcfheaders

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__genesis__null_previous)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filter_headers{ client_filter::type_id::neutrino, 0, hash }, node_version->value);

    const auto message = client_filter_headers::deserialize(node_version->value, receive(client_filter_headers::command));
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->filter_type, client_filter::type_id::neutrino);
    BOOST_REQUIRE(message->stop_hash == hash);
    BOOST_REQUIRE(message->previous_filter_header == null_hash);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__unknown_type__stopped)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filter_headers{ unknown_filter_type, 0, hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__unknown_stop_hash__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filter_headers{ client_filter::type_id::neutrino, 0, one_hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__start_above_stop__stopped)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filter_headers{ client_filter::type_id::neutrino, 1, hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__2000_difference__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filter_headers{ client_filter::type_id::neutrino, 0, descendant(2000).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_headers::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filter_headers__1999_difference_unfiltered__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filter_headers{ client_filter::type_id::neutrino, 0, descendant(1999).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter_headers::command), boost::system::system_error);
}

// getcfilters

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__unknown_type__stopped)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filters{ unknown_filter_type, 0, hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__unknown_stop_hash__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filters{ client_filter::type_id::neutrino, 0, one_hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__start_above_stop__stopped)
{
    BOOST_REQUIRE(handshake());

    const auto hash = config_.bitcoin.genesis_block.hash();
    send(get_client_filters{ client_filter::type_id::neutrino, 1, hash }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__1000_difference__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filters{ client_filter::type_id::neutrino, 0, descendant(1000).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__999_difference_unfiltered__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filters{ client_filter::type_id::neutrino, 0, descendant(999).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_CASE(protocol_filter_out_70015__get_client_filters__unfiltered_stop__stopped)
{
    BOOST_REQUIRE(handshake());

    send(get_client_filters{ client_filter::type_id::neutrino, 0, descendant(1).hash() }, node_version->value);

    BOOST_REQUIRE_THROW(receive(client_filter::command), boost::system::system_error);
}

BOOST_AUTO_TEST_SUITE_END()
