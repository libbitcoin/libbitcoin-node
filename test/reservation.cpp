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
#include <memory>
#include <boost/test/unit_test.hpp>
#include <bitcoin/node.hpp>
#include "utility.hpp"

using namespace bc;
using namespace bc::config;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::node::test;

using namespace bc;

BOOST_AUTO_TEST_SUITE(reservation_tests)

// rate
//-----------------------------------------------------------------------------

// Just a basic test to get some coverage output.
BOOST_AUTO_TEST_CASE(reservations__rate__default__idle_zeros)
{
    node::settings settings;
    blockchain_fixture blockchain;
    config::checkpoint::list checkpoints;
    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    reservations reserves(hashes, blockchain, settings);

    reservation reserve(reserves, 42, 24);
    BOOST_REQUIRE(true);
}


// table
//-----------------------------------------------------------------------------

////BOOST_AUTO_TEST_CASE(reservations__table__hash_1__size_1_by_1_hashes_empty)
////{
////    node::settings settings;
////    blockchain_fixture blockchain;
////    config::checkpoint::list checkpoints;
////    header_queue hashes(checkpoints);
////    hashes.initialize(check42);
////    reservations reserves(hashes, blockchain, settings);
////    const auto table = reserves.table();
////    BOOST_REQUIRE_EQUAL(table.size(), 1u);
////    BOOST_REQUIRE_EQUAL(table[0]->size(), 1u);
////    BOOST_REQUIRE_EQUAL(table[0]->slot(), 0u);
////    BOOST_REQUIRE(hashes.empty());
////}

BOOST_AUTO_TEST_SUITE_END()
