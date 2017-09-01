/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/utility/performance.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace node {

// The allowed number of standard deviations below the norm.
// With 1 channel this multiple is irrelevant, no channels are dropped.
// With 2 channels a < 1.0 multiple will drop a channel on every test.
// With 2 channels a 1.0 multiple will fluctuate based on rounding.
// With 2 channels a > 1.0 multiple will prevent all channel drops.
// With 3+ channels the multiple determines allowed deviation from norm.
static constexpr float multiple = 1.01f;

inline double to_seconds(double rate_microseconds)
{
    static constexpr double micro_per_second = 1000 * 1000;
    return rate_microseconds * micro_per_second;
}

double performance::normal() const
{
    // This is commonly nan when the window and discount are both zero.
    return divide<double>(events, static_cast<double>(window) - discount);
}

double performance::rate() const
{
    return divide<double>(events, window);
}

double performance::ratio() const
{
    return divide<double>(discount, window);
}

bool performance::expired(size_t slot, const statistics& summary) const
{
    const auto normal_rate = normal();
    const auto deviation = normal_rate - summary.arithmentic_mean;
    const auto absolute_deviation = std::fabs(deviation);
    const auto allowed_deviation = multiple * summary.standard_deviation;
    const auto outlier = absolute_deviation > allowed_deviation;
    const auto below_average = deviation < 0;
    const auto expired = below_average && outlier;

    LOG_DEBUG(LOG_NODE)
        << "Statistics for slot (" << slot << ")"
        << " adj:" << (to_seconds(normal_rate))
        << " avg:" << (to_seconds(summary.arithmentic_mean))
        << " dev:" << (to_seconds(deviation))
        << " sdv:" << (to_seconds(summary.standard_deviation))
        << " cnt:" << (summary.active_count)
        << " neg:" << (below_average ? "T" : "F")
        << " out:" << (outlier ? "T" : "F")
        << " exp:" << (expired ? "T" : "F");

    return expired;
}

} // namespace node
} // namespace libbitcoin
