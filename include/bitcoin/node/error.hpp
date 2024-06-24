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
    branch_error,
    orphan_block,
    orphan_header,
    duplicate_block,
    duplicate_header,
    ////validation_bypass,
    ////confirmation_bypass,

    /// chasers
    set_block_unconfirmable,
    get_height,
    get_branch_work,
    get_is_strong,
    invalid_branch_point,
    pop_candidate,
    push_candidate,
    invalid_fork_point,
    get_candidate_chain_state,
    get_block,
    get_unassociated,
    get_fork_work,
    to_confirmed,
    pop_confirmed,
    get_block_confirmable,
    get_block_state,
    set_strong,
    set_unstrong,
    set_organized,
    set_block_confirmable,
    set_block_valid,
    node_confirm,
    node_validate,
    node_roll_back
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

} // namespace error
} // namespace node
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::node::error::error)

#endif
