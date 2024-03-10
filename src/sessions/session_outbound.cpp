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
#include <bitcoin/node/sessions/session_outbound.hpp>

#include <algorithm>
#include <cmath>
#include <ratio>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_outbound

/// Three required to measure deviation.
constexpr auto minimum_deviation_set = 3_size;
constexpr auto to_kilobits_per_second = [](auto value) NOEXCEPT
{
    // There are no system wrappers for floating point conversion.
    BC_PUSH_WARNING(NO_STATIC_CAST)
    BC_PUSH_WARNING(NO_CASTS_FOR_ARITHMETIC_CONVERSION)
    return system::encode_base10(static_cast<uint64_t>(
        (value * byte_bits) / std::kilo::num));
    BC_POP_WARNING()
    BC_POP_WARNING()
};

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

session_outbound::session_outbound(full_node& node,
    uint64_t identifier) NOEXCEPT
  : attach(node, identifier),
    network::tracker<session_outbound>(node.log),
    allowed_deviation_(node.node_settings().allowed_deviation)
{
}

void session_outbound::performance(uint64_t channel, uint64_t speed,
    network::result_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        BIND(do_performance, channel, speed, handler));
}

void session_outbound::do_performance(uint64_t channel, uint64_t speed,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (speed == max_uint64)
    {
        speeds_.erase(channel);
        handler(error::exhausted_channel);
        return;
    }

    // Always remove record on stalled channel (and channel close).
    if (is_zero(speed))
    {
        speeds_.erase(channel);
        handler(error::stalled_channel);
        return;
    }

    speeds_[channel] = static_cast<double>(speed);

    const auto count = speeds_.size();
    if (count <= minimum_deviation_set)
    {
        handler(error::success);
        return;
    }

    const auto rate = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [](double sum, const auto& element) NOEXCEPT
        {
            return sum + element.second;
        });


    // This bypasses unnecessary computation but prevents consistent logging.
    // Keep this channel if its performance deviation is at/above average.
    const auto mean = rate / count;
    ////if (speed >= mean)
    ////{
    ////    handler(error::success);
    ////    return;
    ////}

    const auto variance = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [mean](double sum, const auto& element) NOEXCEPT
        {
            return sum + std::pow(element.second - mean, two);
        }) / (sub1(count));

    const auto sdev = std::sqrt(variance);
    const auto slow = (mean - speed) > (allowed_deviation_ * sdev);

    ////system::string_list out{};
    ////for (const auto& value: speeds_)
    ////    out.push_back(system::serialize(to_kilobits_per_second(value.second)));

    LOGN("Block download channels (" << count << ") rate ("
        << to_kilobits_per_second(rate) << ") mean ("
        << to_kilobits_per_second(mean) << ") sdev ("
        << to_kilobits_per_second(sdev) << ") Kbps [" << (slow ? "*" : "")
        << to_kilobits_per_second(speed) << "].");
        ////<< system::join(out));

    if (slow)
    {
        // Drop this channel if the magnitude of its below average performance
        // deviation exceeds the allowed multiple of standard deviations.
        handler(error::slow_channel);
        return;
    }

    handler(error::success);
}

void session_outbound::attach_protocols(
    const network::channel::ptr& channel) NOEXCEPT
{
    auto& self = *this;
    const auto version = channel->negotiated_version();
    constexpr auto performance = true;

    // Attach appropriate alert, reject, ping, and/or address protocols.
    network::session_outbound::attach_protocols(channel);

    // Very hard to find < 31800 peer to connect with.
    if (version >= network::messages::level::bip130)
    {
        channel->attach<protocol_header_in_70012>(self)->start();
        channel->attach<protocol_header_out_70012>(self)->start();
        channel->attach<protocol_block_in_31800>(self, performance)->start();
    }
    else if (version >= network::messages::level::headers_protocol)
    {
        channel->attach<protocol_header_in_31800>(self)->start();
        channel->attach<protocol_header_out_31800>(self)->start();
        channel->attach<protocol_block_in_31800>(self, performance)->start();
    }
    else
    {
        // Blocks-first synchronization (no header protocol).
        channel->attach<protocol_block_in>(self)->start();
    }

    channel->attach<protocol_block_out>(self)->start();
    channel->attach<protocol_transaction_in>(self)->start();
    channel->attach<protocol_transaction_out>(self)->start();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
