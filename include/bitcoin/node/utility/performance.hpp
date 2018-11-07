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
#ifndef LIBBITCOIN_NODE_PERFORMANCE_HPP
#define LIBBITCOIN_NODE_PERFORMANCE_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/statistics.hpp>

namespace libbitcoin {
namespace node {

class BCN_API performance
{
public:
    // Use microseconds and bytes internally for precision.
    static double to_megabits_per_second(double bytes_per_microsecond);

    /// The event rate, exclusive of discount time.
    double rate() const;

    /// The ratio of discount time to total time.
    double ratio() const;

    /// The standard deviation exceeds allowed multiple.
    bool expired(size_t slot, float maximum_deviation,
        const statistics& summary) const;

    // An idling slot has less than minimum history for calculation.
    bool idle;

    // The number of events measured (e.g. bytes or blocks).
    size_t events;

    // Database cost in microseconds, so we do not count against peer.
    uint64_t discount;

    // Measurement moving window duration in microseconds.
    uint64_t window;
};

// Coerce division into double and error into zero.
template<typename Quotient, typename Dividend, typename Divisor>
static Quotient divide(Dividend dividend, Divisor divisor)
{
    const auto quotient = static_cast<Quotient>(dividend) / divisor;
    return std::isnan(quotient) || std::isinf(quotient) ? 0.0 : quotient;
}

} // namespace node
} // namespace libbitcoin

#endif
