/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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

// static
double performance::to_megabits_per_second(double bytes_per_microsecond)
{
    // Use standard telecom definition of a megabit (125,000 bytes).
    static constexpr auto bytes_per_megabyte = 1000.0 * 1000.0;
    static constexpr auto micro_per_second = 1000.0 * 1000.0;
    static const auto bytes_per_megabit = bytes_per_megabyte / byte_bits;
    return micro_per_second * bytes_per_microsecond / bytes_per_megabit;
}

double performance::rate() const
{
    // This is commonly nan when the window and discount are both zero.
    // This produces a zero rate, but this is ignored as it implies idle.
    return divide<double>(events, static_cast<double>(window) - discount);
}

double performance::ratio() const
{
    return divide<double>(discount, window);
}

bool performance::expired(size_t, float maximum_deviation,
    const statistics& summary) const
{
    const auto normal_rate = rate();
    const auto deviation = normal_rate - summary.arithmentic_mean;
    const auto absolute_deviation = std::fabs(deviation);
    const auto allowed = maximum_deviation * summary.standard_deviation;
    const auto outlier = absolute_deviation > allowed;
    const auto below_average = deviation < 0;
    const auto expired = below_average && outlier;

    ////LOG_VERBOSE(LOG_NODE)
    ////    << "Statistics for slot (" << slot << ")"
    ////    << " adj:" << (to_megabits_per_second(normal_rate))
    ////    << " avg:" << (to_megabits_per_second(summary.arithmentic_mean))
    ////    << " dev:" << (to_megabits_per_second(deviation))
    ////    << " sdv:" << (to_megabits_per_second(summary.standard_deviation))
    ////    << " cnt:" << (summary.active_count)
    ////    << " neg:" << (below_average ? "T" : "F")
    ////    << " out:" << (outlier ? "T" : "F")
    ////    << " exp:" << (expired ? "T" : "F");

    return expired;
}

} // namespace node
} // namespace libbitcoin
