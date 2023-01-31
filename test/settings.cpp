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

BOOST_AUTO_TEST_SUITE(settings_tests)

// [log]

BOOST_AUTO_TEST_CASE(settings__log__default_context__expected)
{
    log::settings configuration{};
    BOOST_REQUIRE_EQUAL(configuration.verbose, false);
    BOOST_REQUIRE_EQUAL(configuration.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(configuration.path, "");
    BOOST_REQUIRE_EQUAL(configuration.file1(), "bn_end.log");
    BOOST_REQUIRE_EQUAL(configuration.file2(), "bn_begin.log");
}

// [node]

BOOST_AUTO_TEST_SUITE_END()
