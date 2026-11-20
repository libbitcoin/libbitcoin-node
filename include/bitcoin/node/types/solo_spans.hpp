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
#ifndef LIBBITCOIN_NODE_TYPES_SOLO_SPANS_HPP
#define LIBBITCOIN_NODE_TYPES_SOLO_SPANS_HPP

#include <atomic>
#include <bitcoin/node/chase.hpp>

namespace libbitcoin {
namespace node {

/// Thread safe spans that may overlap, each told at its end if it was alone.
class BCN_API solo_spans
{
public:
    using ticket = uint64_t;

    /// Begin a span.
    ticket begin() NOEXCEPT;

    /// End the span, true if no other span was open at any time during it.
    bool end(ticket begun) NOEXCEPT;

private:
    // The state holds spans begun (high word) and spans open (low word).
    static constexpr auto one_open = 1_u64;
    static constexpr auto one_begun = system::power2<uint64_t>(
        bits<uint32_t>);

    // This is thread safe.
    std::atomic<uint64_t> state_{};
};

} // namespace node
} // namespace libbitcoin

#endif
