/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_ERROR_HPP
#define LIBBITCOIN_NODE_ERROR_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/version.hpp>

namespace libbitcoin {
namespace node {

/// Alias system code.
/// std::error_code "node" category holds node::error::error_t.
typedef std::error_code code;

namespace error {

/// Asio failures are normalized to the error codes below.
/// Stop by explicit call is mapped to channel_stopped or service_stopped
/// depending on the context. Asio errors returned on cancel calls are ignored.
enum error_t : uint8_t
{
    /// general
    success,

    /// database
    store_uninitialized,
    store_reload,
    store_snapshot,

    /// network
    slow_channel,
    stalled_channel,
    exhausted_channel,
    sacrificed_channel,
    suspended_channel,
    suspended_service,

    /// blockchain
    orphan_block,
    orphan_header,
    duplicate_block,
    duplicate_header,

    /// faults (terminal, code error and store corruption assumed)
    protocol1,
    protocol2,
    header1,
    organize1,
    organize2,
    organize3,
    organize4,
    organize5,
    organize6,
    organize7,
    organize8,
    organize9,
    organize10,
    organize11,
    organize12,
    organize13,
    organize14,
    organize15,
    validate1,
    validate2,
    validate3,
    validate4,
    validate5,
    validate6,
    validate7,
    validate8,
    confirm1,
    confirm2,
    confirm3,
    confirm4,
    confirm5,
    confirm6,
    confirm7,
    confirm8,
    confirm9,
    confirm10,
    confirm11,
    confirm12,
    confirm13
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

} // namespace error
} // namespace node
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::node::error::error)

#endif
