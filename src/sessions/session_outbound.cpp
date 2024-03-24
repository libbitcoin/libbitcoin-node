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
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_outbound

using namespace network;
using namespace std::placeholders;

constexpr auto to_kilobits_per_second = [](auto value) NOEXCEPT
{
    // Floating point conversion.
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
    allowed_deviation_(node.config().node.allowed_deviation)
{
}

// split
// ----------------------------------------------------------------------------

void session_outbound::start(result_handler&& handler) NOEXCEPT
{
    // Events subscription is synchronous (session).
    subscribe_events(BIND(handle_event, _1, _2, _3));

    network::session_outbound::start(std::move(handler));
}

// Event subscriber operates on the network strand (session).
void session_outbound::handle_event(const code&,
    chase event_, event_link value) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    switch (event_)
    {
        case chase::starved:
        {
            // When a channel becomes starved notify other(s) to split work.
            BC_ASSERT(std::holds_alternative<channel_t>(value));
            split(std::get<channel_t>(value));
            break;
        }
        case chase::header:
        case chase::download:
        ////case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::pause:
        case chase::resume:
        case chase::bump:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmed:
        case chase::unpreconfirmed:
        case chase::confirmed:
        case chase::unconfirmed:
        case chase::disorganized:
        case chase::transaction:
        case chase::candidate:
        case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

void session_outbound::split(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto slowest = std::min_element(speeds_.begin(), speeds_.end(),
        [](const auto& left, const auto& right) NOEXCEPT
        {
            return left.second < right.second;
        });

    // Direct the slowest channel to split work and stop.
    if (slowest != speeds_.end())
    {
        // Erase entry so less likely to be claimed again before stopping.
        const auto channel = slowest->first;
        speeds_.erase(slowest);
        node::session::notify(error::success, chase::split, channel);
        return;
    }

    // With no speeds recorded there may still be channels with work.
    node::session::notify(error::success, chase::stall, {});
}

// performance
// ----------------------------------------------------------------------------

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

    // Floating point conversion.
    BC_PUSH_WARNING(NO_STATIC_CAST)
    speeds_[channel] = static_cast<double>(speed);
    BC_POP_WARNING()

    // Three elements are required to measure deviation, don't drop below.
    const auto count = speeds_.size();
    if (count <= minimum_for_standard_deviation)
    {
        handler(error::success);
        return;
    }

    const auto rate = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [](double sum, const auto& element) NOEXCEPT
        {
            return sum + element.second;
        });

    const auto mean = rate / count;
    if (speed >= mean)
    {
        handler(error::success);
        return;
    }

    const auto variance = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [mean](double sum, const auto& element) NOEXCEPT
        {
            return sum + std::pow(element.second - mean, two);
        }) / (sub1(count));

    const auto sdev = std::sqrt(variance);
    const auto slow = (mean - speed) > (allowed_deviation_ * sdev);

    // Only speed < mean channels are logged.
    LOGN("Block download channels (" << count << ") rate ("
        << to_kilobits_per_second(rate) << ") mean ("
        << to_kilobits_per_second(mean) << ") sdev ("
        << to_kilobits_per_second(sdev) << ") Kbps [" << (slow ? "*" : "")
        << to_kilobits_per_second(speed) << "].");

    if (slow)
    {
        handler(error::slow_channel);
        return;
    }

    handler(error::success);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
