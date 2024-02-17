/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(chaser_tests)

class chaser_header_accessor
  : chaser_header
{
public:
    using chaser_header::chaser_header;

    const network::wall_clock::duration& currency_window_() const NOEXCEPT
    {
        return currency_window();
    }

    bool use_currency_window_() const NOEXCEPT
    {
        return use_currency_window();
    }
};

BOOST_AUTO_TEST_CASE(chaser_test__currency_window__zero__use_currency_window_false)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 0;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    chaser_header_accessor instance(node);
    BOOST_REQUIRE(!instance.use_currency_window_());
}

BOOST_AUTO_TEST_CASE(chaser_test__currency_window__nonzero__use_currency_window_true)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 60;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    chaser_header_accessor instance(node);
    BOOST_REQUIRE(instance.use_currency_window_());
}

BOOST_AUTO_TEST_SUITE_END()
