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
#ifndef LIBBITCOIN_NODE_TYPES_POOL_SCAN_HPP
#define LIBBITCOIN_NODE_TYPES_POOL_SCAN_HPP

#include <bitcoin/node/chase.hpp>

namespace libbitcoin {
namespace node {

/// Scans of the pool.
enum class pool_scan : uint8_t
{
    /// Coverage of an incoming set.
    coverage,

    /// Index write of an admitted tx.
    admission,

    /// Index transition of an organized block.
    block_in,

    /// Index transition of a reorganized block.
    block_out,

    /// Index sweep of the pool context.
    context,

    /// Template.
    template_,

    /// Fee projection.
    projection,

    /// Confirmable txs or fees.
    confirmable
};

} // namespace node
} // namespace libbitcoin

#endif
