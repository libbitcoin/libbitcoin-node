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
#ifndef LIBBITCOIN_NODE_TYPES_POOL_EVENT_HPP
#define LIBBITCOIN_NODE_TYPES_POOL_EVENT_HPP

#include <bitcoin/node/chase.hpp>
#include <bitcoin/node/types/pool_scan.hpp>

namespace libbitcoin {
namespace node {

/// A scan of the pool and what it did.
struct pool_event
{
    pool_scan scan{};
    system::pool::report_t report{};
};

} // namespace node
} // namespace libbitcoin

#endif
