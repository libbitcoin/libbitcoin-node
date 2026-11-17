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
#include "test.hpp"

BC_AUTO_TEST_SUITE(solo_spans_tests)

BC_AUTO_TEST_CASE(solo_spans__end__single__alone)
{
    solo_spans instance{};
    const auto ticket = instance.begin();
    BC_REQUIRE(instance.end(ticket));
}

BC_AUTO_TEST_CASE(solo_spans__end__sequential__both_alone)
{
    solo_spans instance{};
    const auto first = instance.begin();
    const auto first_alone = instance.end(first);
    const auto second = instance.begin();
    const auto second_alone = instance.end(second);
    BC_REQUIRE(first_alone);
    BC_REQUIRE(second_alone);
}

BC_AUTO_TEST_CASE(solo_spans__end__nested__neither_alone)
{
    solo_spans instance{};
    const auto outer = instance.begin();
    const auto inner = instance.begin();
    const auto inner_alone = instance.end(inner);
    const auto outer_alone = instance.end(outer);
    BC_REQUIRE(!inner_alone);
    BC_REQUIRE(!outer_alone);
}

BC_AUTO_TEST_CASE(solo_spans__end__interleaved__neither_alone)
{
    solo_spans instance{};
    const auto first = instance.begin();
    const auto second = instance.begin();
    const auto first_alone = instance.end(first);
    const auto second_alone = instance.end(second);
    BC_REQUIRE(!first_alone);
    BC_REQUIRE(!second_alone);
}

BC_AUTO_TEST_CASE(solo_spans__end__after_overlap__alone)
{
    solo_spans instance{};
    const auto first = instance.begin();
    const auto second = instance.begin();
    instance.end(first);
    instance.end(second);
    const auto third = instance.begin();
    BC_REQUIRE(instance.end(third));
}

BC_AUTO_TEST_SUITE_END()
