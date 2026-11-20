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
#ifndef LIBBITCOIN_NODE_TYPES_ESTIMATE_MODE_HPP
#define LIBBITCOIN_NODE_TYPES_ESTIMATE_MODE_HPP

#include <bitcoin/node/chase.hpp>

namespace libbitcoin {
namespace node {

/// Estimate types.
enum class estimate_mode
{
    /// The pool as it is, with no arrivals.
    basic,

    /// Arrivals weighted by recency, each block half of the one above it.
    geometric,

    /// Arrivals at their mean over the economical window.
    economical,

    /// Arrivals at their peak over the conservative window.
    conservative,

    unknown
};

} // namespace node
} // namespace libbitcoin

#endif
