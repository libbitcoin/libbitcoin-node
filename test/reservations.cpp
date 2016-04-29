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
#include "reservations.hpp"

#include <algorithm>
#include <memory>
#include <boost/test/unit_test.hpp>
#include <bitcoin/node.hpp>

using namespace bc;
using namespace bc::config;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::node::test;

BOOST_AUTO_TEST_SUITE(reservations_tests)

#define DECLARE_RESERVATIONS(name, import) \
config::checkpoint::list checkpoints; \
header_queue hashes(checkpoints); \
blockchain_fixture blockchain(import); \
node::settings settings; \
reservations name(hashes, blockchain, settings)

static const checkpoint check42{ "4242424242424242424242424242424242424242424242424242424242424242", 42 };
static const checkpoint::list no_checks;
static const checkpoint::list one_check{ check42 };

// Create a headers message of specified size, using specified previous hash.
static message::headers::ptr message_factory(size_t count, const hash_digest& hash)
{
    auto previous_hash = hash;
    const auto headers = std::make_shared<message::headers>();
    auto& elements = headers->elements;

    for (size_t height = 0; height < count; ++height)
    {
        const header current_header{ 0, previous_hash, {}, 0, 0, 0, 0 };
        elements.push_back(current_header);
        previous_hash = current_header.hash();
    }

    return headers;
}

// Create a headers message of specified size, starting with a genesis header.
static message::headers::ptr message_factory(size_t count)
{
    return message_factory(count, null_hash);
}

// max_request
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__max_request__default__50000)
{
    DECLARE_RESERVATIONS(reserves, true);
    BOOST_REQUIRE_EQUAL(reserves.max_request(), 50000u);
}

BOOST_AUTO_TEST_CASE(reservations__set_max_request__42__42)
{
    DECLARE_RESERVATIONS(reserves, true);
    reserves.set_max_request(42);
    BOOST_REQUIRE_EQUAL(reserves.max_request(), 42u);
}

// import
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__import__true__true)
{
    const auto block_ptr = std::make_shared<block>();
    DECLARE_RESERVATIONS(reserves, true);
    BOOST_REQUIRE(reserves.import(block_ptr, 42));
}

BOOST_AUTO_TEST_CASE(reservations__import__false__false)
{
    const auto block_ptr = std::make_shared<block>();
    DECLARE_RESERVATIONS(reserves, false);
    BOOST_REQUIRE(!reserves.import(block_ptr, 42));
}

// table
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__table__default__empty)
{
    DECLARE_RESERVATIONS(reserves, true);
    BOOST_REQUIRE(reserves.table().empty());
}

BOOST_AUTO_TEST_CASE(reservations__table__hash_1__size_1_by_1_hashes_empty)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 1u);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[0]->slot(), 0u);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(reservations__table__hash_4__size_4_by_1_hashes_empty)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(3, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 4u);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[3]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[0]->slot(), 0u);
    BOOST_REQUIRE_EQUAL(table[1]->slot(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->slot(), 2u);
    BOOST_REQUIRE_EQUAL(table[3]->slot(), 3u);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(reservations__table__connections_5_hash_46__size_5_by_9_hashes_1)
{
    node::settings settings;
    settings.download_connections = 5;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(45, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 5u);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 9u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 9u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 9u);
    BOOST_REQUIRE_EQUAL(table[3]->size(), 9u);
    BOOST_REQUIRE_EQUAL(table[4]->size(), 9u);
    BOOST_REQUIRE_EQUAL(table[0]->slot(), 0u);
    BOOST_REQUIRE_EQUAL(table[1]->slot(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->slot(), 2u);
    BOOST_REQUIRE_EQUAL(table[3]->slot(), 3u);
    BOOST_REQUIRE_EQUAL(table[4]->slot(), 4u);
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(reservations__table__hash_42__size_8_by_5_hashes_2)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(41, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 8u);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[3]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[4]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[5]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[6]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[7]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[0]->slot(), 0u);
    BOOST_REQUIRE_EQUAL(table[1]->slot(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->slot(), 2u);
    BOOST_REQUIRE_EQUAL(table[3]->slot(), 3u);
    BOOST_REQUIRE_EQUAL(table[4]->slot(), 4u);
    BOOST_REQUIRE_EQUAL(table[5]->slot(), 5u);
    BOOST_REQUIRE_EQUAL(table[6]->slot(), 6u);
    BOOST_REQUIRE_EQUAL(table[7]->slot(), 7u);
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
}

// remove
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__remove__empty__does_not_throw)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 1u);

    const auto row = table[0];
    reserves.remove(row);
    BOOST_REQUIRE(reserves.table().empty());
    BOOST_REQUIRE_NO_THROW(reserves.remove(row));
}

BOOST_AUTO_TEST_CASE(reservations__remove__hash_4__size_3)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(3, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table1 = reserves.table();
    BOOST_REQUIRE_EQUAL(table1.size(), 4u);
    BOOST_REQUIRE(hashes.empty());

    const auto row = table1[2];
    BOOST_REQUIRE_EQUAL(row->slot(), 2u);

    reserves.remove(row);
    const auto table2 = reserves.table();
    BOOST_REQUIRE_EQUAL(table2.size(), 3u);
    BOOST_REQUIRE_EQUAL(table2[0]->slot(), 0u);
    BOOST_REQUIRE_EQUAL(table2[1]->slot(), 1u);
    BOOST_REQUIRE_EQUAL(table2[2]->slot(), 3u);
}

// populate
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_available__uncapped)
{
    node::settings settings;
    settings.download_connections = 3;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(10, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 3u);

    // All rows have three hashes.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u);

    // The reserved hashes are transfered to the row.
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
    BOOST_REQUIRE(reserves.populate(table[1]));
    BOOST_REQUIRE_EQUAL(hashes.size(), 0u);

    // The row is increased by the reserve amount.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 5u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u);
}

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_available__capped)
{
    node::settings settings;
    settings.download_connections = 3;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(9, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 3u);

    // Cap the reserves below the level of the row allocation.
    reserves.set_max_request(2);

    // All rows have three hashes.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u);

    // The exsting population is greater than the max request, so no reserve.
    // The row is not empty so must not cause a repartitioning.
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
    BOOST_REQUIRE(reserves.populate(table[1]));
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);

    // All rows still have three hashes.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u);
}

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_empty__no_population)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(3, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 4u);

    // All rows have one hash.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[3]->size(), 1u);

    // There are no hashes in reserve.
    BOOST_REQUIRE(hashes.empty());

    // The row is not empty so must not cause a repartitioning.
    BOOST_REQUIRE(reserves.populate(table[0]));

    // Partitions remain unchanged.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u);
    BOOST_REQUIRE_EQUAL(table[3]->size(), 1u);
}

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_empty_empty_table__no_partition)
{
    node::settings settings;
    settings.download_connections = 3;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);

    // Initialize with a known header so we can import its block later.
    const auto message = message_factory(3, null_hash);
    auto& elements = message->elements;
    const auto genesis_header = elements[0];
    hashes.initialize(genesis_header.hash(), 0);
    elements.erase(std::find(elements.begin(), elements.end(), elements[0]));
    BOOST_REQUIRE_EQUAL(elements.size(), 2u);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 3u);

    // There are no hashes in reserve.
    BOOST_REQUIRE(hashes.empty());

    // Declare blocks that hash to the allocated headers.
    // Blocks are evenly distrubuted (every third to each row).
    const auto block0 = std::make_shared<block>(block{ genesis_header });
    const auto block1 = std::make_shared<block>(block{ elements[0] });
    const auto block2 = std::make_shared<block>(block{ elements[1] });

    // All rows have one hash.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 0
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 1
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u); // 2

    // Remvoe all rows from the member table.
    reserves.remove(table[0]);
    reserves.remove(table[1]);
    reserves.remove(table[2]);
    BOOST_REQUIRE(reserves.table().empty());

    // Removing a block from the first row of the cached table must result in
    // one less hash in that row and no partitioning of other rows, since they
    // are no longer accessible from the member table.
    table[0]->import(block0);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 0u); //
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 1
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u); // 2
}

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_empty__partition)
{
    node::settings settings;
    settings.download_connections = 3;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);

    // Initialize with a known header so we can import its block later.
    const auto message = message_factory(9, null_hash);
    auto& elements = message->elements;
    const auto genesis_header = elements[0];
    hashes.initialize(genesis_header.hash(), 0);
    elements.erase(std::find(elements.begin(), elements.end(), elements[0]));
    BOOST_REQUIRE_EQUAL(elements.size(), 8u);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 3u);

    // There are no hashes in reserve.
    BOOST_REQUIRE(hashes.empty());

    // Declare blocks that hash to the allocated headers.
    // Blocks are evenly distrubuted (every third to each row).
    const auto block0 = std::make_shared<block>(block{ genesis_header });
    const auto block1 = std::make_shared<block>(block{ elements[0] });
    const auto block2 = std::make_shared<block>(block{ elements[1] });
    const auto block3 = std::make_shared<block>(block{ elements[2] });
    const auto block4 = std::make_shared<block>(block{ elements[3] });
    const auto block5 = std::make_shared<block>(block{ elements[4] });
    const auto block6 = std::make_shared<block>(block{ elements[5] });
    const auto block7 = std::make_shared<block>(block{ elements[6] });
    const auto block8 = std::make_shared<block>(block{ elements[7] });
    const auto block9 = std::make_shared<block>(block{ elements[8] });

    // A row becomes stopped once empty.
    BOOST_REQUIRE(!table[0]->stopped());
    BOOST_REQUIRE(!table[1]->stopped());
    BOOST_REQUIRE(!table[2]->stopped());

    // All rows have three hashes.
    BOOST_REQUIRE_EQUAL(table[0]->size(), 3u); // 0/3/6
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u); // 1/4/7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u); // 2/5/8

    // Remove a block from the first row.
    table[0]->import(block0);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 2u); // 3/6
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u); // 1/4/7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u); // 2/5/8

    // Remove another block from the first row.
    table[0]->import(block3);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 6
    BOOST_REQUIRE_EQUAL(table[1]->size(), 3u); // 1/4/7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u); // 2/5/8

    // Removing the last block from the first row results in partitioning of
    // of the highst row (row 1 winds the tie with row 2 due to ordering).
    // Half of the row 1 allocation is moved to row 0, rounded up to 2 hashes.
    table[0]->import(block6);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 2u); // 1/4
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u); // 2/5/8

    // Remove another block from the first row (originally from the second).
    table[0]->import(block1);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 4
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 3u); // 2/5/8

    // Remove another block from the first row (originally from the second).
    table[0]->import(block4);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 2u); // 2/5
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u); // 8

    // Remove another block from the first row (originally from the third).
    table[0]->import(block2);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 5
    BOOST_REQUIRE_EQUAL(table[1]->size(), 1u); // 7
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u); // 8

    // Remove another block from the first row (originally from the third).
    table[0]->import(block5);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 7
    BOOST_REQUIRE_EQUAL(table[1]->size(), 0u); //
    BOOST_REQUIRE_EQUAL(table[2]->size(), 1u); //
    BOOST_REQUIRE(table[1]->stopped());

    // Remove another block from the first row (originally from the second).
    table[0]->import(block7);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u); // 8
    BOOST_REQUIRE_EQUAL(table[1]->size(), 0u); //
    BOOST_REQUIRE_EQUAL(table[2]->size(), 0u); //
    BOOST_REQUIRE(table[2]->stopped());

    // Remove another block from the first row (originally from the third).
    table[0]->import(block8);
    BOOST_REQUIRE_EQUAL(table[0]->size(), 0u); //
    BOOST_REQUIRE_EQUAL(table[1]->size(), 0u); //
    BOOST_REQUIRE_EQUAL(table[2]->size(), 0u); //
    BOOST_REQUIRE(table[0]->stopped());

    // We can't test the partition aspect of population directly
    // because there is no way to reduce the row count to empty.
    ////BOOST_REQUIRE(reserves.populate(table[0]));
}

// rates
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__rates__default__zeros)
{
    DECLARE_RESERVATIONS(reserves, true);
    const auto rates = reserves.rates();
    BOOST_REQUIRE_EQUAL(rates.active_count, 0u);
    BOOST_REQUIRE_EQUAL(rates.arithmentic_mean, 0.0);
    BOOST_REQUIRE_EQUAL(rates.standard_deviation, 0.0);
}

BOOST_AUTO_TEST_CASE(reservations__rates__three_reservations_same_rates__no_deviation)
{
    node::settings settings;
    settings.download_connections = 3;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(2, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 3u);

    const auto rates1 = reserves.rates();
    BOOST_REQUIRE_EQUAL(rates1.active_count, 0u);
    BOOST_REQUIRE_EQUAL(rates1.arithmentic_mean, 0.0);
    BOOST_REQUIRE_EQUAL(rates1.standard_deviation, 0.0);

    // normalized rates: 5 / (2 - 1) = 5
    performance rate0;
    rate0.idle = false;
    rate0.events = 5;
    rate0.database = 1;
    rate0.window = 2;
    performance rate1 = rate0;
    performance rate2 = rate0;

    // Simulate the rate summary on each channel by setting it directly.
    table[0]->set_rate(rate0);
    table[1]->set_rate(rate1);
    table[2]->set_rate(rate2);

    const auto rates2 = reserves.rates();

    // There are three active (non-idle) rows.
    BOOST_REQUIRE_EQUAL(rates2.active_count, 3u);

    // mean: (5 + 5 + 5) / 3 = 5
    BOOST_REQUIRE_EQUAL(rates2.arithmentic_mean, 5.0);

    // deviations: { 5-5=0, 5-5=0, 5-5=0 }
    // variance: (0^2 + 0^2 + 0^2) / 3 = 0
    // standard deviation: sqrt(0)
    BOOST_REQUIRE_EQUAL(rates2.standard_deviation, 0.0);
}

BOOST_AUTO_TEST_CASE(reservations__rates__five_reservations_one_idle__idle_excluded)
{
    node::settings settings;
    settings.download_connections = 5;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(4, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();

    // normalized rate: 5 / (2 - 1) = 5
    performance rate0;
    rate0.idle = false;
    rate0.events = 5;
    rate0.database = 1;
    rate0.window = 2;

    // This rate is idle, so values must be excluded in rates computation.
    performance rate1;
    rate1.idle = true;
    rate1.events = 42;
    rate1.database = 42;
    rate1.window = 42;

    // normalized rate: 10 / (6 - 1) = 2
    performance rate2;
    rate2.idle = false;
    rate2.events = 10;
    rate2.database = 1;
    rate2.window = 6;

    // normalized rate: 3 / (6 - 3) = 1
    performance rate3;
    rate3.idle = false;
    rate3.events = 3;
    rate3.database = 3;
    rate3.window = 6;

    // normalized rate: 8 / (5 - 3) = 4
    performance rate4;
    rate4.idle = false;
    rate4.events = 8;
    rate4.database = 3;
    rate4.window = 5;

    // Simulate the rate summary on each channel by setting it directly.
    table[0]->set_rate(rate0);
    table[1]->set_rate(rate1);
    table[2]->set_rate(rate2);
    table[3]->set_rate(rate3);
    table[4]->set_rate(rate4);

    const auto rates2 = reserves.rates();

    // There are three active (non-idle) rows.
    BOOST_REQUIRE_EQUAL(rates2.active_count, 4u);

    // mean: (5 + 2 + 1 + 4) / 4 = 3
    BOOST_REQUIRE_EQUAL(rates2.arithmentic_mean, 3.0);

    // deviations: { 3-5=-2, 3-2=1, 3-1=-2, 3-4=-1 }
    // variance: ((-2)^2 + 1^2 + 2^2 + (-1)^2) / 4 = 2.5
    // standard deviation: sqrt(2.5)
    BOOST_REQUIRE_EQUAL(rates2.standard_deviation, std::sqrt(2.5));
}

BOOST_AUTO_TEST_SUITE_END()
