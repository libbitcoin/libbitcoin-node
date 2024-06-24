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
#include <bitcoin/node/error.hpp>

#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {
namespace error {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // database
    { store_uninitialized, "store not initialized" },
    { store_reload, "store reload" },
    { store_snapshot, "store snapshot" },

    // network
    { slow_channel, "slow channel" },
    { stalled_channel, "stalled channel" },
    { exhausted_channel, "exhausted channel" },
    { sacrificed_channel, "sacrificed channel" },
    { suspended_channel, "sacrificed channel" },
    { suspended_service, "sacrificed service" },

    // blockchain
    { branch_error, "branch error" },
    { orphan_block, "orphan block" },
    { orphan_header, "orphan header" },
    { duplicate_block, "duplicate block" },
    { duplicate_header, "duplicate header" },
    ////{ validation_bypass, "validation bypass" },
    ////{ confirmation_bypass, "confirmation bypass" },

    /// query
    { set_block_unconfirmable, "set_block_unconfirmable" },
    { get_height, "get_height" },
    { get_branch_work, "get_branch_work" },
    { get_is_strong, "get_is_strong" },
    { invalid_branch_point, "invalid_branch_point" },
    { pop_candidate, "pop_candidate" },
    { push_candidate, "push_candidate" },
    { invalid_fork_point, "invalid_fork_point" },
    { get_candidate_chain_state, "get_candidate_chain_state" },
    { get_block, "get_block" },
    { get_unassociated, "get_unassociated" },
    { get_fork_work, "get_fork_work" },
    { to_confirmed, "to_confirmed" },
    { pop_confirmed, "pop_confirmed" },
    { get_block_confirmable, "get_block_confirmable" },
    { get_block_state, "get_block_state" },
    { set_strong, "set_strong" },
    { set_unstrong, "set_unstrong" },
    { set_organized, "set_organized" },
    { set_block_confirmable, "set_block_confirmable" },
    { set_block_valid, "set_block_valid" },

    /// query composite
    { node_confirm, "node_confirm" },
    { node_validate, "node_validate" },
    { node_roll_back, "node_roll_back" }
};

DEFINE_ERROR_T_CATEGORY(error, "node", "node code")

} // namespace error
} // namespace node
} // namespace libbitcoin
