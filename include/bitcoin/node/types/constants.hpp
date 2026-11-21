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
#ifndef LIBBITCOIN_NODE_TYPES_CONSTANTS_HPP
#define LIBBITCOIN_NODE_TYPES_CONSTANTS_HPP

#include <bitcoin/node/chase.hpp>

namespace libbitcoin {
namespace node {

constexpr size_t maximum_estimate_horizon = 1008;

/// Blocks of arrivals (two hours and one day at ten minutes per block).
constexpr size_t economical_estimate_window = 12;
constexpr size_t conservative_estimate_window = 144;

/// bip152: high bandwidth mode is requested of at most three peers.
constexpr size_t maximum_compact_peers = 3;

/// Relay slots, one bit of a pooled row's marks each (none is relay_slots).
constexpr size_t relay_slots = bits<uint64_t>;

} // namespace node
} // namespace libbitcoin

#endif
