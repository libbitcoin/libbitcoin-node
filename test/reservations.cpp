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

BOOST_AUTO_TEST_CASE(reservations__table__hash_1__size_1_hashes_empty)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    reservations reserves(hashes, blockchain, settings);
    BOOST_REQUIRE_EQUAL(reserves.table().size(), 1u);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(reservations__table__hash_4__size_4_hashes_empty)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(3, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    BOOST_REQUIRE_EQUAL(reserves.table().size(), 4u);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(reservations__table__connections_5_hash_46__size_5_hashes_1)
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
    BOOST_REQUIRE_EQUAL(reserves.table().size(), 5u);
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(reservations__table__hash_42__size_8_hashes_2)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(41, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    BOOST_REQUIRE_EQUAL(reserves.table().size(), 8u);
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
}

// remove
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__remove__empty__empty)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    hashes.initialize(check42);

    reservations reserves(hashes, blockchain, settings);
    const auto table1 = reserves.table();
    BOOST_REQUIRE_EQUAL(table1.size(), 1u);
    BOOST_REQUIRE(hashes.empty());

    const auto row = table1[0];
    BOOST_REQUIRE_EQUAL(row->slot(), 0u);

    reserves.remove(row);
    BOOST_REQUIRE(reserves.table().empty());
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

// populate
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_empty__partition)
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
    BOOST_REQUIRE(hashes.empty());

    const auto row = table[0];
    BOOST_REQUIRE(reserves.populate(row));
}

BOOST_AUTO_TEST_CASE(reservations__populate__hashes_available__reserve)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    const auto message = message_factory(8, check42.hash());
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));

    reservations reserves(hashes, blockchain, settings);
    const auto table = reserves.table();
    BOOST_REQUIRE_EQUAL(table.size(), 8u);
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);

    const auto row = table[0];
    BOOST_REQUIRE(reserves.populate(row));
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_SUITE_END()
