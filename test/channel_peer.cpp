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
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A node peer channel over an unconnected socket.
struct channel_peer_setup_fixture
{
    DELETE_COPY_MOVE(channel_peer_setup_fixture);

    channel_peer_setup_fixture(uint16_t announcement_cache)
      : config_{ system::chain::selection::mainnet },
        channel_
        {
            [&]() NOEXCEPT
            {
                config_.node.announcement_cache = announcement_cache;
                network::socket::parameters params
                {
                    .maximum_request = 42,
                    .maximum_buffer = 42
                };

                const auto socket = std::make_shared<network::socket>(log_,
                    pool_.service(), std::move(params));
                return std::make_shared<node::channel_peer>(log_, socket, 42,
                    config_, options);
            }()
        }
    {
    }

    ~channel_peer_setup_fixture()
    {
        channel_->stop(network::error::service_stopped);
        pool_.stop();
        BOOST_REQUIRE(pool_.join());
    }

    /// Announce first then second, report whether each was announced.
    std::pair<bool, bool> announce(const system::hash_digest& first,
        const system::hash_digest& second)
    {
        std::promise<std::pair<bool, bool>> promise{};
        boost::asio::post(channel_->strand(), [&]() NOEXCEPT
        {
            channel_->set_announced(first);
            channel_->set_announced(second);
            const auto announced1 = channel_->was_announced(first);
            const auto announced2 = channel_->was_announced(second);
            promise.set_value({ announced1, announced2 });
        });

        return promise.get_future().get();
    }

    static inline const network::channel_peer::options_t options{ "test" };

protected:
    const network::logger log_{};
    network::threadpool pool_{ 1 };
    configuration config_;
    node::channel_peer::ptr channel_;
};

struct channel_peer_disabled_setup_fixture
  : channel_peer_setup_fixture
{
    channel_peer_disabled_setup_fixture()
      : channel_peer_setup_fixture(0)
    {
    }
};

struct channel_peer_one_setup_fixture
  : channel_peer_setup_fixture
{
    channel_peer_one_setup_fixture()
      : channel_peer_setup_fixture(1)
    {
    }
};

struct channel_peer_two_setup_fixture
  : channel_peer_setup_fixture
{
    channel_peer_two_setup_fixture()
      : channel_peer_setup_fixture(2)
    {
    }
};

static const system::hash_digest first{ 0x01 };
static const system::hash_digest second{ 0x02 };

BOOST_AUTO_TEST_SUITE(channel_peer_tests)

BOOST_FIXTURE_TEST_CASE(channel_peer__set_announced__zero_cache__not_announced, channel_peer_disabled_setup_fixture)
{
    const auto announced = announce(first, second);
    BOOST_REQUIRE(!announced.first);
    BOOST_REQUIRE(!announced.second);
}

BOOST_FIXTURE_TEST_CASE(channel_peer__set_announced__full_cache__oldest_evicted, channel_peer_one_setup_fixture)
{
    const auto announced = announce(first, second);
    BOOST_REQUIRE(!announced.first);
    BOOST_REQUIRE(announced.second);
}

BOOST_FIXTURE_TEST_CASE(channel_peer__set_announced__cache_not_full__both_announced, channel_peer_two_setup_fixture)
{
    const auto announced = announce(first, second);
    BOOST_REQUIRE(announced.first);
    BOOST_REQUIRE(announced.second);
}

BOOST_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
