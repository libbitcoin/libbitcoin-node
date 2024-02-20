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

BOOST_AUTO_TEST_SUITE(chaser_header_tests)

class mock_chaser_header
  : chaser_header
{
public:
    using chaser_header::chaser_header;

    const auto& tree() NOEXCEPT
    {
        return tree_;
    }

    const network::wall_clock::duration& currency_window() const NOEXCEPT
    {
        return chaser_header::currency_window();
    }

    bool use_currency_window() const NOEXCEPT
    {
        return chaser_header::use_currency_window();
    }

    code start() NOEXCEPT override
    {
        return chaser_header::start();
    }

    void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT override
    {
        return chaser_header::handle_event(ec, event_, value);
    }

    bool get_branch_work(uint256_t& work, size_t& point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT override
    {
        return chaser_header::get_branch_work(work, point, tree_branch,
            store_branch, header);
    }

    bool get_is_strong(bool& strong, const uint256_t& work,
        size_t point) const NOEXCEPT override
    {
        return chaser_header::get_is_strong(strong, work, point);
    }

    bool is_current(const system::chain::header& header,
        size_t height) const NOEXCEPT override
    {
        return chaser_header::is_current(header, height);
    }

    void save(const system::chain::header::cptr& header,
        const system::chain::context& context) NOEXCEPT override
    {
        return chaser_header::save(header, context);
    }

    database::header_link push(const system::chain::header::cptr& header,
        const system::chain::context& context) const NOEXCEPT override
    {
        return chaser_header::push(header, context);
    }

    bool push(const system::hash_digest& key) NOEXCEPT override
    {
        return chaser_header::push(key);
    }
};

BOOST_AUTO_TEST_CASE(chaser_header_test__currency_window__zero__use_currency_window_false)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 0;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    mock_chaser_header instance(node);
    BOOST_REQUIRE(!instance.use_currency_window());
}

BOOST_AUTO_TEST_CASE(chaser_header_test__currency_window__nonzero__use_currency_window_true)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 60;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    mock_chaser_header instance(node);
    BOOST_REQUIRE(instance.use_currency_window());
}

BOOST_AUTO_TEST_CASE(chaser_header_test__is_current__zero_currency_window__true)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 0;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    mock_chaser_header instance(node);

    BOOST_REQUIRE(instance.is_current(system::chain::header{ {}, {}, {}, 0, {}, {} }, 0));
    BOOST_REQUIRE(instance.is_current(system::chain::header{ {}, {}, {}, max_uint32, {}, {} }, 0));
}

BOOST_AUTO_TEST_CASE(chaser_header_test__is_current__one_minute_currency_window__expected)
{
    const network::logger log{};
    node::configuration config(system::chain::selection::mainnet);
    config.node.currency_window_minutes = 1;

    full_node::store store(config.database);
    full_node::query query(store);
    full_node node(query, config, log);
    mock_chaser_header instance(node);

    BOOST_REQUIRE(!instance.is_current(system::chain::header{ {}, {}, {}, 0, {}, {} }, 0));
    BOOST_REQUIRE(instance.is_current(system::chain::header{ {}, {}, {}, max_uint32, {}, {} }, 0));
}

BOOST_AUTO_TEST_SUITE_END()
