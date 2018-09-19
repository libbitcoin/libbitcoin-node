/////**
//// * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
//// *
//// * This file is part of libbitcoin.
//// *
//// * This program is free software: you can redistribute it and/or modify
//// * it under the terms of the GNU Affero General Public License as published by
//// * the Free Software Foundation, either version 3 of the License, or
//// * (at your option) any later version.
//// *
//// * This program is distributed in the hope that it will be useful,
//// * but WITHOUT ANY WARRANTY; without even the implied warranty of
//// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// * GNU Affero General Public License for more details.
//// *
//// * You should have received a copy of the GNU Affero General Public License
//// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//// */
////#include <chrono>
////#include <memory>
////#include <utility>
////#include <boost/test/unit_test.hpp>
////#include <bitcoin/node.hpp>
////#include "utility.hpp"
////
////using namespace bc;
////using namespace bc::config;
////using namespace bc::message;
////using namespace bc::node;
////using namespace bc::node::test;
////
////BOOST_AUTO_TEST_SUITE(reservation_tests)
////
////// slot
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__slot__construct_42__42)
////{
////    reservations reserves;
////    const size_t slot = 42;
////    reservation reserve(reserves, slot, 0);
////    BOOST_REQUIRE(reserve.empty());
////    BOOST_REQUIRE_EQUAL(reserve.slot(), slot);
////}
////
////// empty
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__empty__default__true)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    BOOST_REQUIRE(reserve.empty());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__empty__one_hash__false)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.insert(hash_digest{ check42.hash() }, check42.height());
////    BOOST_REQUIRE(!reserve.empty());
////}
////
////// size
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__size__default__0)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    BOOST_REQUIRE_EQUAL(reserve.size(), 0u);
////}
////
////BOOST_AUTO_TEST_CASE(reservation__size__one_hash__1)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.insert(hash_digest{ check42.hash() }, check42.height());
////    BOOST_REQUIRE_EQUAL(reserve.size(), 1u);
////}
////
////BOOST_AUTO_TEST_CASE(reservation__size__duplicate_hash__1)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.insert(hash_digest{ check42.hash() }, check42.height());
////    reserve.insert(hash_digest{ check42.hash() }, check42.height());
////    BOOST_REQUIRE_EQUAL(reserve.size(), 1u);
////}
////
////// stopped
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__stopped__default__false)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    BOOST_REQUIRE(!reserve.stopped());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__stopped__one_hash__false)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.insert(hash_digest{ check42.hash() }, check42.height());
////    BOOST_REQUIRE(!reserve.stopped());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__stopped__import_last_block__true)
////{
////    reservations reserves;
////    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
////    const auto message = message_factory(1, check42.hash());
////    const auto& header1 = message->elements()[0];
////    reserve->insert(header1.hash(), 42);
////    const auto block1 = std::make_shared<const block>(block{ header1, {} });
////    reserve->import(block1);
////    BOOST_REQUIRE(reserve->empty());
////    BOOST_REQUIRE(reserve->stopped());
////}
////
////// rate
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__rate__default__defaults)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    const auto rate = reserve.rate();
////    BOOST_REQUIRE(rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 0u);
////    BOOST_REQUIRE_EQUAL(rate.database, 0u);
////    BOOST_REQUIRE_EQUAL(rate.window, 0u);
////}
////
////// set_rate
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__set_rate__values__expected)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    performance value;
////    value.idle = false;
////    value.events = 1;
////    value.database = 2;
////    value.window = 3;
////    reserve.set_rate(std::move(value));
////    const auto rate = reserve.rate();
////    BOOST_REQUIRE(!rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 1u);
////    BOOST_REQUIRE_EQUAL(rate.database, 2u);
////    BOOST_REQUIRE_EQUAL(rate.window, 3u);
////}
////
////// pending
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__pending__default__true)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    BOOST_REQUIRE(reserve.pending());
////}
////
////// set_pending
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__set_pending__false_true__false_true)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.set_pending(false);
////    BOOST_REQUIRE(!reserve.pending());
////    reserve.set_pending(true);
////    BOOST_REQUIRE(reserve.pending());
////}
////
////// rate_window
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__rate_window__construct_10__30_seconds)
////{
////    reservations reserves;
////    const size_t expected = 10;
////    reservation reserve(reserves, 0, expected);
////    const auto window = reserve.rate_window();
////    BOOST_REQUIRE_EQUAL(window.count(), expected * 1000 * 1000 * 3);
////}
////
////// reset
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__reset__values__defaults)
////{
////    reservations reserves;
////
////    // The timeout cannot be exceeded because the current time is fixed.
////    static const uint32_t timeout = 1;
////    const auto now = std::chrono::high_resolution_clock::now();
////    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);
////
////    // Create a history entry.
////    const auto message = message_factory(3, null_hash);
////    reserve->insert(message->elements()[0].hash(), 0);
////    const auto block0 = std::make_shared<const block>(block{ message->elements()[0], {} });
////    const auto block1 = std::make_shared<const block>(block{ message->elements()[1], {} });
////    const auto block2 = std::make_shared<const block>(block{ message->elements()[2], {} });
////    reserve->import(block0);
////    reserve->import(block1);
////
////    // Idle checks assume minimum_history is set to 3.
////    BOOST_REQUIRE(reserve->idle());
////
////    // Set rate.
////    reserve->set_rate({ false, 1, 2, 3 });
////
////    // Clear rate and history.
////    reserve->reset();
////
////    // Confirm reset of rate.
////    const auto rate = reserve->rate();
////    BOOST_REQUIRE(rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 0u);
////    BOOST_REQUIRE_EQUAL(rate.database, 0u);
////    BOOST_REQUIRE_EQUAL(rate.window, 0u);
////
////    // Confirm clearance of history (non-idle indicated with third history).
////    reserve->import(block2);
////    BOOST_REQUIRE(reserve->idle());
////}
////
////// idle
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__idle__default__true)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    BOOST_REQUIRE(reserve.idle());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__idle__set_false__false)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    reserve.set_rate({ false, 1, 2, 3 });
////    BOOST_REQUIRE(!reserve.idle());
////}
////
////// insert
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__insert1__single__size_1_pending)
////{
////    reservations reserves;
////    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, 0);
////    const auto message = message_factory(1, check42.hash());
////    const auto& header = message->elements()[0];
////    BOOST_REQUIRE(reserve->empty());
////    reserve->set_pending(false);
////    reserve->insert(checkpoint{ header.hash(), 42 });
////    BOOST_REQUIRE_EQUAL(reserve->size(), 1u);
////    BOOST_REQUIRE(reserve->pending());
////}
////
////// TODO: verify pending.
////BOOST_AUTO_TEST_CASE(reservation__insert2__single__size_1_pending)
////{
////    reservations reserves;
////    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, 0);
////    const auto message = message_factory(1, check42.hash());
////    const auto& header = message->elements()[0];
////    BOOST_REQUIRE(reserve->empty());
////    reserve->set_pending(false);
////    reserve->insert(header.hash(), 42);
////    BOOST_REQUIRE_EQUAL(reserve->size(), 1u);
////    BOOST_REQUIRE(reserve->pending());
////}
////
////// import
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__import__unsolicitied___empty_idle)
////{
////    reservations reserves;
////    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
////    const auto message = message_factory(1, check42.hash());
////    const auto& header = message->elements()[0];
////    const auto block1 = std::make_shared<const block>(block{ header, {} });
////    BOOST_REQUIRE(reserve->idle());
////    reserve->import(block1);
////    BOOST_REQUIRE(reserve->idle());
////    BOOST_REQUIRE(reserve->empty());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__import__fail__idle)
////{
////    reservations reserves;
////    const auto reserve = std::make_shared<reservation>(reserves, 0, 0);
////    const auto message = message_factory(1, check42.hash());
////    const auto& header = message->elements()[0];
////    reserve->insert(header.hash(), 42);
////    const auto block1 = std::make_shared<const block>(block{ header, {} });
////    BOOST_REQUIRE(reserve->idle());
////    reserve->import(block1);
////    BOOST_REQUIRE(reserve->idle());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__import__three_success_timeout__idle)
////{
////    reservations reserves;
////
////    // If import time is non-zero the zero timeout will exceed and history will not accumulate.
////    static const uint32_t timeout = 0;
////    const auto now = std::chrono::high_resolution_clock::now();
////    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);
////    const auto message = message_factory(3, null_hash);
////    reserve->insert(message->elements()[0].hash(), 0);
////    reserve->insert(message->elements()[1].hash(), 1);
////    reserve->insert(message->elements()[2].hash(), 2);
////    const auto block0 = std::make_shared<const block>(block{ message->elements()[0], {} });
////    const auto block1 = std::make_shared<const block>(block{ message->elements()[1], {} });
////    const auto block2 = std::make_shared<const block>(block{ message->elements()[2], {} });
////    reserve->import(block0);
////    reserve->import(block1);
////    reserve->import(block2);
////
////    // Idle checks assume minimum_history is set to 3.
////    BOOST_REQUIRE(reserve->idle());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__import__three_success__not_idle)
////{
////    reservations reserves;
////
////    // The timeout cannot be exceeded because the current time is fixed.
////    static const uint32_t timeout = 1;
////    const auto now = std::chrono::high_resolution_clock::now();
////    const auto reserve = std::make_shared<reservation_fixture>(reserves, 0, timeout, now);
////    const auto message = message_factory(3, null_hash);
////    reserve->insert(message->elements()[0].hash(), 0);
////    reserve->insert(message->elements()[1].hash(), 1);
////    reserve->insert(message->elements()[2].hash(), 2);
////    const auto block0 = std::make_shared<const block>(block{ message->elements()[0], {} });
////    const auto block1 = std::make_shared<const block>(block{ message->elements()[1], {} });
////    const auto block2 = std::make_shared<const block>(block{ message->elements()[2], {} });
////
////    // Idle checks assume minimum_history is set to 3.
////    BOOST_REQUIRE(reserve->idle());
////    reserve->import(block0);
////    BOOST_REQUIRE(reserve->idle());
////    reserve->import(block1);
////    BOOST_REQUIRE(reserve->idle());
////    reserve->import(block2);
////    BOOST_REQUIRE(!reserve->idle());
////}
////
////// toggle_partitioned
//////-----------------------------------------------------------------------------
////
////// see reservations__populate__hashes_empty__partition for positive test.
////BOOST_AUTO_TEST_CASE(reservation__toggle_partitioned__default__false_pending)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    BOOST_REQUIRE(!reserve.toggle_partitioned());
////    BOOST_REQUIRE(reserve.pending());
////}
////
////// partition
//////-----------------------------------------------------------------------------
////
////// see reservations__populate__ for positive tests.
////BOOST_AUTO_TEST_CASE(reservation__partition__minimal_not_empty__false_unchanged)
////{
////    reservations reserves;
////    const auto reserve1 = std::make_shared<reservation_fixture>(reserves, 0, 0);
////    const auto reserve2 = std::make_shared<reservation_fixture>(reserves, 1, 0);
////    reserve2->insert(check42);
////    BOOST_REQUIRE(reserve1->partition(reserve2));
////    BOOST_REQUIRE_EQUAL(reserve2->size(), 1u);
////}
////
////// request
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__request__pending__empty_not_reset)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    reserve.set_rate({ false, 1, 2, 3 });
////    BOOST_REQUIRE(reserve.pending());
////
////    // Creates a request with no hashes reserved.
////    const auto result = reserve.request(false);
////    BOOST_REQUIRE(result.inventories().empty());
////    BOOST_REQUIRE(!reserve.pending());
////
////    // The rate is not reset because the new channel parameter is false.
////    const auto rate = reserve.rate();
////    BOOST_REQUIRE(!rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 1u);
////    BOOST_REQUIRE_EQUAL(rate.database, 2u);
////    BOOST_REQUIRE_EQUAL(rate.window, 3u);
////}
////
////BOOST_AUTO_TEST_CASE(reservation__request__new_channel_pending__size_1_reset)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    const auto message = message_factory(1, null_hash);
////    reserve.insert(message->elements()[0].hash(), 0);
////    reserve.set_rate({ false, 1, 2, 3 });
////    BOOST_REQUIRE(reserve.pending());
////
////    // Creates a request with one hash reserved.
////    const auto result = reserve.request(true);
////    BOOST_REQUIRE_EQUAL(result.inventories().size(), 1u);
////    BOOST_REQUIRE(result.inventories()[0].hash() == message->elements()[0].hash());
////    BOOST_REQUIRE(!reserve.pending());
////
////    // The rate is reset because the new channel parameter is true.
////    const auto rate = reserve.rate();
////    BOOST_REQUIRE(rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 0u);
////    BOOST_REQUIRE_EQUAL(rate.database, 0u);
////    BOOST_REQUIRE_EQUAL(rate.window, 0u);
////}
////
////BOOST_AUTO_TEST_CASE(reservation__request__new_channel__size_1_reset)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    const auto message = message_factory(1, null_hash);
////    reserve.insert(message->elements()[0].hash(), 0);
////    reserve.set_rate({ false, 1, 2, 3 });
////    reserve.set_pending(false);
////
////    // Creates a request with one hash reserved.
////    const auto result = reserve.request(true);
////    BOOST_REQUIRE_EQUAL(result.inventories().size(), 1u);
////    BOOST_REQUIRE(result.inventories()[0].hash() == message->elements()[0].hash());
////    BOOST_REQUIRE(!reserve.pending());
////
////    // The rate is reset because the new channel parameter is true.
////    const auto rate = reserve.rate();
////    BOOST_REQUIRE(rate.idle);
////    BOOST_REQUIRE_EQUAL(rate.events, 0u);
////    BOOST_REQUIRE_EQUAL(rate.database, 0u);
////    BOOST_REQUIRE_EQUAL(rate.window, 0u);
////}
////
////BOOST_AUTO_TEST_CASE(reservation__request__three_hashes_pending__size_3)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    const auto message = message_factory(3, null_hash);
////    reserve.insert(message->elements()[0].hash(), 0);
////    reserve.insert(message->elements()[1].hash(), 1);
////    reserve.insert(message->elements()[2].hash(), 2);
////    BOOST_REQUIRE(reserve.pending());
////
////    // Creates a request with 3 hashes reserved.
////    const auto result = reserve.request(false);
////    BOOST_REQUIRE_EQUAL(result.inventories().size(), 3u);
////    BOOST_REQUIRE(result.inventories()[0].hash() == message->elements()[0].hash());
////    BOOST_REQUIRE(result.inventories()[1].hash() == message->elements()[1].hash());
////    BOOST_REQUIRE(result.inventories()[2].hash() == message->elements()[2].hash());
////    BOOST_REQUIRE(!reserve.pending());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__request__one_hash__empty)
////{
////    reservations reserves;
////    reservation_fixture reserve(reserves, 0, 0);
////    const auto message = message_factory(1, null_hash);
////    reserve.insert(message->elements()[0].hash(), 0);
////    reserve.set_pending(false);
////
////    // Creates an empty request for not new and not pending scneario.
////    const auto result = reserve.request(false);
////    BOOST_REQUIRE(result.inventories().empty());
////    BOOST_REQUIRE(!reserve.pending());
////}
////
////// expired
//////-----------------------------------------------------------------------------
////
////BOOST_AUTO_TEST_CASE(reservation__expired__default__false)
////{
////    reservations reserves;
////    reservation reserve(reserves, 0, 0);
////    BOOST_REQUIRE(!reserve.expired());
////}
////
////BOOST_AUTO_TEST_CASE(reservation__expired__various__expected)
////{
////    static const auto minimum_peers = 5u;
////    static const auto block_latency_seconds = 1u;
////    blockchain_fixture blockchain;
////    config::checkpoint::list checkpoints;
////    ////header_queue hashes(checkpoints);
////    const auto message = message_factory(4, check42.hash());
////    ////hashes.initialize(check42);
////    ////BOOST_REQUIRE(hashes.enqueue(message));
////    reservations reserves;
////    const auto table = reserves.table();
////
////    // Simulate the rate summary on each channel by setting it directly.
////
////    // normalized rate: 5 / (2 - 1) = 5
////    table[0]->set_rate({ false,  5,  1,  2 });
////
////    // normalized rate: 42 / (42 - 42) = 0
////    // This rate is idle, so values must be excluded in rates computation.
////    table[1]->set_rate({ true,  42, 42, 42 });
////
////    // normalized rate: 10 / (6 - 1) = 2
////    table[2]->set_rate({ false, 10,  1,  6 });
////
////    // normalized rate: 3 / (6 - 3) = 1
////    table[3]->set_rate({ false,  3,  3,  6 });
////
////    // normalized rate: 8 / (5 - 3) = 4
////    table[4]->set_rate({ false,  8,  3,  5 });
////
////    // see reservations__rates__five_reservations_one_idle__idle_excluded
////    const auto rates2 = reserves.rates();
////    BOOST_REQUIRE_EQUAL(rates2.active_count, 4u);
////    BOOST_REQUIRE_EQUAL(rates2.arithmentic_mean, 3.0);
////
////    // standard deviation: ~ 1.58
////    BOOST_REQUIRE_EQUAL(rates2.standard_deviation, std::sqrt(2.5));
////
////    // deviation: 5 - 3 = +2
////    BOOST_REQUIRE(!table[0]->expired());
////
////    // deviation: 0 - 3 = -3
////    BOOST_REQUIRE(table[1]->expired());
////
////    // deviation: 2 - 3 = -1
////    BOOST_REQUIRE(!table[2]->expired());
////
////    // deviation: 1 - 3 = -2
////    BOOST_REQUIRE(table[3]->expired());
////
////    // deviation: 4 - 3 = +1
////    BOOST_REQUIRE(!table[4]->expired());
////}
////
////BOOST_AUTO_TEST_SUITE_END()
