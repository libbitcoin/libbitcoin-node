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

    channel_peer_setup_fixture()
      : config_{ test::node_configuration() }, channel_{ make_channel() }
    {
    }

    ~channel_peer_setup_fixture()
    {
        channel_->stop(network::error::service_stopped);
        pool_.stop();
        BC_REQUIRE(pool_.join());
    }

    node::channel_peer::ptr make_channel() NOEXCEPT
    {
        using namespace system;
        using socket = network::socket;
        using channel = node::channel_peer;
        socket::parameters params
        {
            .maximum_request = 42,
            .maximum_buffer = 42
        };

        auto& io = pool_.service();
        const auto wire = emplace_shared<socket>(log_, io, std::move(params));
        return emplace_shared<channel>(log_, wire, 42, config_, options);
    }

    // A distinct uniform hash for each index.
    static system::hash_digest hash(size_t index)
    {
        using namespace system;
        return sha256_hash(to_little_endian<uint64_t>(index));
    }

    // Announce the hash on the channel strand.
    void announce(const system::hash_digest& hash)
    {
        std::promise<bool> promise{};
        network::io::post(channel_->strand(), [&]() NOEXCEPT
        {
            channel_->set_announced(hash);
            promise.set_value(true);
        });

        BC_REQUIRE(promise.get_future().get());
    }

    // The hash is reported announced on the channel strand.
    bool announced(const system::hash_digest& hash)
    {
        std::promise<bool> promise{};
        network::io::post(channel_->strand(), [&]() NOEXCEPT
        {
            promise.set_value(channel_->was_announced(hash));
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

BC_FIXTURE_TEST_SUITE(channel_peer_tests, channel_peer_setup_fixture)

BC_AUTO_TEST_CASE(channel_peer__was_announced__empty__false)
{
    const auto item = hash(0);
    BC_REQUIRE(!announced(item));
}

BC_AUTO_TEST_CASE(channel_peer__set_announced__one__announced)
{
    const auto item = hash(0);
    announce(item);
    BC_REQUIRE(announced(item));
}

BC_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
