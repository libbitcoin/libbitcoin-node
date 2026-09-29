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
    static const chain::transaction instance
    {
        1u,
        chain::inputs{ { chain::point{ one_hash, 0u }, chain::script{}, max_uint32 } },
        chain::outputs{ { parent_value, chain::script{ chain::operations{ { chain::opcode::push_positive_1 } } } } },
        0u
    };

    return instance;
}

static chain::transaction::cptr spend(const hash_digest& hash, uint64_t value, uint32_t locktime=0, uint32_t sequence=max_uint32) NOEXCEPT
{
    return std::make_shared<const chain::transaction>(chain::transaction
    {
        1u,
        chain::inputs{ { chain::point{ hash, 0u }, chain::script{}, sequence } },
        chain::outputs{ { value, chain::script{ chain::operations{ { chain::opcode::push_positive_1 } } } } },
        locktime
    });
}

static chain::transaction::cptr spend(uint64_t value) NOEXCEPT
{
    return spend(parent().hash(false), value);
}

static chain::transactions_cptr package(const chain::transaction_cptrs& txs) NOEXCEPT
{
    return std::make_shared<const chain::transaction_cptrs>(txs);
}

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

struct chaser_transaction_setup_fixture
  : p2p_setup_fixture
{
    using result = std::pair<code, size_t>;

    inline chaser_transaction_setup_fixture(const configurator& configure={})
      : p2p_setup_fixture([](node::query& query)
        {
            return query.set(parent());
        }, configure)
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

    const auto tx = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ tx })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().type == type_id::transaction);
    BOOST_REQUIRE(message->items.front().hash == tx->hash(false));
}

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__submit__below_fee_filter__not_announced, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::maximum_protocol, true));

    send(peer::fee_filter{ 1'000'000 }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

    const auto low = spend(parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ low })).first, node::error::success);

    send(peer::fee_filter{ 0 }, node_version->value);
    BOOST_REQUIRE(!received_before_pong(peer::inventory::command));

    const auto next = spend(low->hash(false), parent_value);
    BOOST_REQUIRE_EQUAL(submit(package({ next })).first, node::error::success);

    const auto payload = receive(peer::inventory::command);
    const auto message = peer::inventory::deserialize(node_version->value, payload);
    BOOST_REQUIRE(message);
    BOOST_REQUIRE_EQUAL(message->items.size(), one);
    BOOST_REQUIRE(message->items.front().hash == next->hash(false));
}

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

BOOST_FIXTURE_TEST_CASE(protocol_transaction_out__get_data__unknown_tx_106__stopped, chaser_transaction_pooling_setup_fixture)
{
    BOOST_REQUIRE(handshake(0, peer::level::bip35, true));

    send(peer::get_data{ { { type_id::transaction, one_hash } } }, peer::level::bip35);
    BOOST_REQUIRE_THROW(receive(peer::not_found::command), boost::system::system_error);
}

BOOST_AUTO_TEST_SUITE_END()
