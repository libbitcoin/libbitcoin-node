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
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_outbound
    
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

session_outbound::session_outbound(full_node& node,
    uint64_t identifier) NOEXCEPT
  : attach(node, identifier),
    network::tracker<session_outbound>(node.log)
{
};

void session_outbound::performance(uint64_t channel, uint64_t speed,
    network::result_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        BIND3(do_performance, channel, speed, handler));
}

void session_outbound::do_performance(uint64_t channel, uint64_t speed,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "session_outbound");

    // Always remove record on stalled channel (and channel close).
    if (is_zero(speed))
    {
        speeds_.erase(channel);
        handler(error::stalled_channel);
        return;
    }

    speeds_[channel] = static_cast<double>(speed);

    const auto size = speeds_.size();
    const auto mean = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [](double sum, const auto& element) NOEXCEPT
        {
            return sum + element.second;
        }) / size;

    // Keep this channel if its performance deviation is at/above average.
    if (speed >= mean)
    {
        handler(error::success);
        return;
    }

    const auto variance = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [mean](double sum, const auto& element) NOEXCEPT
        {
            const auto difference = element.second - mean;
            return sum + (difference * difference);
        }) / size;

    const auto standard_deviation = std::sqrt(variance);
    const auto allowed_deviation = config().node.allowed_deviation;

    // Drop this channel if the magnitude of its below average performance
    // deviation exceeds the allowed multiple of standard deviations.
    const auto slow = (mean - speed) > (allowed_deviation * standard_deviation);
    handler(slow ? error::slow_channel : error::success);
}

void session_outbound::attach_protocols(
    const network::channel::ptr& channel) NOEXCEPT
{
    // Attach appropriate alert, reject, ping, and/or address protocols.
    network::session_outbound::attach_protocols(channel);

    auto& self = *this;
    ////const auto version = channel->negotiated_version();

    ////if (version >= network::messages::level::bip130)
    ////{
    ////    channel->attach<protocol_header_in_70012>(self)->start();
    ////    channel->attach<protocol_header_out_70012>(self)->start();
    ////}
    ////else if (version >= network::messages::level::headers_protocol)
    ////{
    ////    channel->attach<protocol_header_in_31800>(self)->start();
    ////    channel->attach<protocol_header_out_31800>(self)->start();
    ////}

    channel->attach<protocol_block_in>(self)->start();
    ////channel->attach<protocol_block_out>(self)->start();
    ////channel->attach<protocol_transaction_in>(self)->start();
    ////channel->attach<protocol_transaction_out>(self)->start();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
