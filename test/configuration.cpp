/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include "test.hpp"

using namespace bc::system;

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    node::configuration instance(chain::selection::none);
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.initchain);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE_EQUAL(instance.log.verbose, false);
    BOOST_REQUIRE_EQUAL(instance.log.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(instance.log.path, "");
    BOOST_REQUIRE_EQUAL(instance.log.file1(), "node1.log");
    BOOST_REQUIRE_EQUAL(instance.log.file2(), "node2.log");
}

BOOST_AUTO_TEST_SUITE_END()
