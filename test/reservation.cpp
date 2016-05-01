/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <chrono>
#include <memory>
#include <boost/test/unit_test.hpp>
#include <bitcoin/node.hpp>
#include "utility.hpp"

using namespace bc;
using namespace bc::config;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::node::test;

BOOST_AUTO_TEST_SUITE(reservation_tests)

// slot
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__slot__construct_42__42)
{
    DECLARE_RESERVATIONS(reserves, true);
    const size_t expected = 42;
    reservation reserve(reserves, expected, 0);
    BOOST_REQUIRE(reserve.empty());
}

// empty
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__empty__default__true)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE(reserve.empty());
}

BOOST_AUTO_TEST_CASE(reservation__empty__one_hash__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    reserve.insert(check42);
    BOOST_REQUIRE(!reserve.empty());
}

// size
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__size__default__0)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE_EQUAL(reserve.size(), 0u);
}

BOOST_AUTO_TEST_CASE(reservation__size__one_hash__1)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    reserve.insert(check42);
    BOOST_REQUIRE_EQUAL(reserve.size(), 1u);
}

BOOST_AUTO_TEST_CASE(reservation__size__duplicate_hash__1)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    reserve.insert(check42);
    reserve.insert(check42);
    BOOST_REQUIRE_EQUAL(reserve.size(), 1u);
}

// stopped
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__stopped__default__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE(!reserve.stopped());
}

BOOST_AUTO_TEST_CASE(reservation__stopped__one_hash__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    reserve.insert(check42);
    BOOST_REQUIRE(!reserve.stopped());
}

BOOST_AUTO_TEST_CASE(reservation__stopped__import_last_block__true)
{
    DECLARE_RESERVATIONS(reserves, true);
    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
    const auto message = message_factory(1, check42.hash());
    const auto& header1 = message->elements[0];
    reserve->insert(header1.hash(), 42);
    const auto block1 = std::make_shared<block>(block{ header1 });
    reserve->import(block1);
    BOOST_REQUIRE(reserve->empty());
    BOOST_REQUIRE(reserve->stopped());
}

// rate
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__rate__default__defaults)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    const auto rate = reserve.rate();
    BOOST_REQUIRE(rate.idle);
    BOOST_REQUIRE_EQUAL(rate.events, 0u);
    BOOST_REQUIRE_EQUAL(rate.database, 0u);
    BOOST_REQUIRE_EQUAL(rate.window, 0u);
}

// set_rate
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__set_rate__values__expected)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    performance value;
    value.idle = false;
    value.events = 1;
    value.database = 2;
    value.window = 3;
    reserve.set_rate(value);
    const auto rate = reserve.rate();
    BOOST_REQUIRE(!rate.idle);
    BOOST_REQUIRE_EQUAL(rate.events, 1u);
    BOOST_REQUIRE_EQUAL(rate.database, 2u);
    BOOST_REQUIRE_EQUAL(rate.window, 3u);
}

// rate_window
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__rate_window__construct_10__30_seconds)
{
    DECLARE_RESERVATIONS(reserves, true);
    const size_t expected = 10;
    reservation_fixture reserve(reserves, 0, expected);
    const auto window = reserve.rate_window();
    BOOST_REQUIRE_EQUAL(window.count(), expected * 1000 * 1000 * 3);
}

// reset
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__reset__values__defaults)
{
    DECLARE_RESERVATIONS(reserves, true);

    // The timeout cannot be exceeded because the current time is fixed.
    static const uint32_t timeout = 1;
    const auto now = std::chrono::high_resolution_clock::now();
    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);

    // Create a history entry.
    const auto message = message_factory(3, null_hash);
    reserve->insert(message->elements[0].hash(), 0);
    const auto block0 = std::make_shared<block>(block{ message->elements[0] });
    const auto block1 = std::make_shared<block>(block{ message->elements[1] });
    const auto block2 = std::make_shared<block>(block{ message->elements[2] });
    reserve->import(block0);
    reserve->import(block1);

    // Idle checks assume minimum_history is set to 3.
    BOOST_REQUIRE(reserve->idle());

    // Set rate.
    reserve->set_rate({ false, 1, 2, 3 });

    // Clear rate and history.
    reserve->reset();

    // Confirm reset of rate.
    const auto rate = reserve->rate();
    BOOST_REQUIRE(rate.idle);
    BOOST_REQUIRE_EQUAL(rate.events, 0u);
    BOOST_REQUIRE_EQUAL(rate.database, 0u);
    BOOST_REQUIRE_EQUAL(rate.window, 0u);

    // Confirm clearance of history (non-idle indicated with third history).
    reserve->import(block2);
    BOOST_REQUIRE(reserve->idle());
}

// idle
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__idle__default__true)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE(reserve.idle());
}

BOOST_AUTO_TEST_CASE(reservation__idle__set_false__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    reserve.set_rate({ false, 1, 2, 3 });
    BOOST_REQUIRE(!reserve.idle());
}

// insert
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__insert1__single__size_1)
{
    DECLARE_RESERVATIONS(reserves, false);
    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
    const auto message = message_factory(1, check42.hash());
    const auto& header = message->elements[0];
    BOOST_REQUIRE(reserve->empty());
    reserve->insert(checkpoint{ header.hash(), 42 });
    BOOST_REQUIRE_EQUAL(reserve->size(), 1u);
}

// TODO: verify pending.
BOOST_AUTO_TEST_CASE(reservation__insert2__single__size_1)
{
    DECLARE_RESERVATIONS(reserves, false);
    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
    const auto message = message_factory(1, check42.hash());
    const auto& header = message->elements[0];
    BOOST_REQUIRE(reserve->empty());
    reserve->insert(header.hash(), 42);
    BOOST_REQUIRE_EQUAL(reserve->size(), 1u);
}

// import
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__import__unsolicitied___empty_idle)
{
    DECLARE_RESERVATIONS(reserves, true);
    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
    const auto message = message_factory(1, check42.hash());
    const auto& header = message->elements[0];
    const auto block1 = std::make_shared<block>(block{ header });
    BOOST_REQUIRE(reserve->idle());
    reserve->import(block1);
    BOOST_REQUIRE(reserve->idle());
    BOOST_REQUIRE(reserve->empty());
}

BOOST_AUTO_TEST_CASE(reservation__import__fail__idle)
{
    DECLARE_RESERVATIONS(reserves, false);
    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
    const auto message = message_factory(1, check42.hash());
    const auto& header = message->elements[0];
    reserve->insert(header.hash(), 42);
    const auto block1 = std::make_shared<block>(block{ header });
    BOOST_REQUIRE(reserve->idle());
    reserve->import(block1);
    BOOST_REQUIRE(reserve->idle());
}

BOOST_AUTO_TEST_CASE(reservation__import__three_success_timeout__idle)
{
    DECLARE_RESERVATIONS(reserves, true);

    // If import time is non-zero the zero timeout will exceed and history will not accumulate.
    static const uint32_t timeout = 0;
    const auto now = std::chrono::high_resolution_clock::now();
    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);
    const auto message = message_factory(3, null_hash);
    reserve->insert(message->elements[0].hash(), 0);
    reserve->insert(message->elements[1].hash(), 1);
    reserve->insert(message->elements[2].hash(), 2);
    const auto block0 = std::make_shared<block>(block{ message->elements[0] });
    const auto block1 = std::make_shared<block>(block{ message->elements[1] });
    const auto block2 = std::make_shared<block>(block{ message->elements[2] });
    reserve->import(block0);
    reserve->import(block1);
    reserve->import(block2);

    // Idle checks assume minimum_history is set to 3.
    BOOST_REQUIRE(reserve->idle());
}

BOOST_AUTO_TEST_CASE(reservation__import__three_success__not_idle)
{
    DECLARE_RESERVATIONS(reserves, true);

    // The timeout cannot be exceeded because the current time is fixed.
    static const uint32_t timeout = 1;
    const auto now = std::chrono::high_resolution_clock::now();
    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);
    const auto message = message_factory(3, null_hash);
    reserve->insert(message->elements[0].hash(), 0);
    reserve->insert(message->elements[1].hash(), 1);
    reserve->insert(message->elements[2].hash(), 2);
    const auto block0 = std::make_shared<block>(block{ message->elements[0] });
    const auto block1 = std::make_shared<block>(block{ message->elements[1] });
    const auto block2 = std::make_shared<block>(block{ message->elements[2] });

    // Idle checks assume minimum_history is set to 3.
    BOOST_REQUIRE(reserve->idle());
    reserve->import(block0);
    BOOST_REQUIRE(reserve->idle());
    reserve->import(block1);
    BOOST_REQUIRE(reserve->idle());
    reserve->import(block2);
    BOOST_REQUIRE(!reserve->idle());
}

// toggle_partitioned
//-----------------------------------------------------------------------------

// see: reservations__populate__hashes_empty__partition for positive test.
BOOST_AUTO_TEST_CASE(reservation__toggle_partitioned__default__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE(!reserve.toggle_partitioned());
}

// request
//-----------------------------------------------------------------------------

// TODO: test pending, new_channel, empty, non_empty, unset pending.
BOOST_AUTO_TEST_CASE(reservation__request__default_new_channel__empty)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    const auto result = reserve.request(true);
    BOOST_REQUIRE(result.inventories.empty());
}

// expired
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservation__expired__default__false)
{
    DECLARE_RESERVATIONS(reserves, true);
    reservation reserve(reserves, 0, 0);
    BOOST_REQUIRE(!reserve.expired());
}

////// TODO: test complex calculation.
////BOOST_AUTO_TEST_CASE(reservation__expired__default__false42)
////{
////    node::settings settings;
////    settings.download_connections = 5;
////    blockchain_fixture blockchain;
////    config::checkpoint::list checkpoints;
////    header_queue hashes(checkpoints);
////    const auto message = message_factory(4, check42.hash());
////    hashes.initialize(check42);
////    BOOST_REQUIRE(hashes.enqueue(message));
////
////    reservations reserves(hashes, blockchain, settings);
////    const auto table = reserves.table();
////
////    // normalized rate: 5 / (2 - 1) = 5
////    performance rate0;
////    rate0.idle = false;
////    rate0.events = 5;
////    rate0.database = 1;
////    rate0.window = 2;
////
////    // This rate is idle, so values must be excluded in rates computation.
////    performance rate1;
////    rate1.idle = true;
////    rate1.events = 42;
////    rate1.database = 42;
////    rate1.window = 42;
////
////    // normalized rate: 10 / (6 - 1) = 2
////    performance rate2;
////    rate2.idle = false;
////    rate2.events = 10;
////    rate2.database = 1;
////    rate2.window = 6;
////
////    // normalized rate: 3 / (6 - 3) = 1
////    performance rate3;
////    rate3.idle = false;
////    rate3.events = 3;
////    rate3.database = 3;
////    rate3.window = 6;
////
////    // normalized rate: 8 / (5 - 3) = 4
////    performance rate4;
////    rate4.idle = false;
////    rate4.events = 8;
////    rate4.database = 3;
////    rate4.window = 5;
////
////    // Simulate the rate summary on each channel by setting it directly.
////    table[0]->set_rate(rate0);
////    table[1]->set_rate(rate1);
////    table[2]->set_rate(rate2);
////    table[3]->set_rate(rate3);
////    table[4]->set_rate(rate4);
////
////    const auto rates2 = reserves.rates();
////
////    // There are three active (non-idle) rows.
////    BOOST_REQUIRE_EQUAL(rates2.active_count, 4u);
////
////    // mean: (5 + 2 + 1 + 4) / 4 = 3
////    BOOST_REQUIRE_EQUAL(rates2.arithmentic_mean, 3.0);
////
////    // deviations: { 3-5=-2, 3-2=1, 3-1=-2, 3-4=-1 }
////    // variance: ((-2)^2 + 1^2 + 2^2 + (-1)^2) / 4 = 2.5
////    // standard deviation: sqrt(2.5)
////    BOOST_REQUIRE_EQUAL(rates2.standard_deviation, std::sqrt(2.5));
////}

BOOST_AUTO_TEST_SUITE_END()
