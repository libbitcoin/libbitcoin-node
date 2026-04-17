/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASE_HPP
#define LIBBITCOIN_NODE_CHASE_HPP

#include <bitcoin/node/events.hpp>

namespace libbitcoin {
namespace node {

enum class chase
{
    /// Work shuffling.
    /// -----------------------------------------------------------------------

    /// Chasers directed to start operating (height_t).
    /// Issued by 'full_node' and handled by 'check', 'validate', 'confirm'.
    start,

    /// Disk space is limited (count_t).
    /// Issued by 'full_node' and handled by 'snapshot' and 'storage'.
    space,

    /// Take a snapshot (height_t).
    /// Issued by 'confirm' and handled by 'snapshot'.
    snap,

    /// Chaser directed to attempt start from its current position (height_t).
    /// Issued by 'organize' and handled by 'check', 'validate', 'confirm'.
    bump,

    /// Channels (all) directed to stop (default).
    /// Issued by 'full_node' and handled by 'observer'.
    suspend,

    /// Chasers (all) directed to resume following suspend (default).
    /// Issued by 'full_node' and handled by 'check', 'validate', 'confirm'.
    resume,

    /// Channel starved for work (object_t).
    /// Issued by 'block_in_31800' and handled by 'session_outbound'.
    starved,

    /// Channel (slow) directed to split work and stop (object_t).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    split,

    /// Channels (all with work) directed to split work and stop (peer_t).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    stall,

    /// Channels (all with work) directed to drop work and stop (peer_t).
    /// Issued by 'check' and handled by 'block_in_31800'.
    purge,

    /// Channels (all) directed to write work count to the log (count_t).
    /// Issued by 'executor' and handled by 'block_in_31800'.
    report,

    /// Candidate Chain.
    /// -----------------------------------------------------------------------

    /// A new candidate branch exists from given branch point (height_t).
    /// Issued by 'block' and handled by 'confirm' and 'snapshot'.
    blocks,

    /// A new candidate branch exists from given branch point (height_t).
    /// Issued by 'header' and handled by 'check'.
    headers,

    /// New candidate headers without txs exist (count_t).
    /// Issued by 'check' and handled by 'block_in_31800'.
    download,

    /// The candidate chain has been reorganized (branched below its top).
    /// Issued by 'organize' and handled by 'check', 'validate', 'confirm'.
    regressed,

    /// unchecked, unvalid or unconfirmable was handled (height_t).
    /// Issued by 'organize' and handled by 'check', 'validate', 'confirm'.
    disorganized,

    /// Check/Identify.
    /// -----------------------------------------------------------------------

    /// A block has been downloaded, checked and stored (height_t).
    /// Issued by 'block_in_31800', handled by 'check', 'validate', 'snapshot'.
    /// Populate is bypassed for checkpoint/milestone blocks.
    checked,

    /// A downloaded block has failed check (header_t).
    /// Issued by 'block_in_31800' and handled by 'organize'.
    unchecked,

    /// Accept/Connect.
    /// -----------------------------------------------------------------------

    /// A branch has become valid (height_t).
    /// Issued by 'validate' and handled by 'check', 'confirm', 'snapshot'.
    valid,

    /// A checked block has failed validation (header_t).
    /// Issued by 'validate' and handled by 'organize'.
    unvalid,

    /// Confirm (block).
    /// -----------------------------------------------------------------------

    /// A connected block has become confirmable (header_t).
    /// Issued by 'confirm' and handled by 'snapshot'.
    confirmable,

    /// A connected block has failed confirmability (header_t).
    /// Issued by 'confirm' and handled by 'organize'.
    unconfirmable,

    /// Confirm (chain).
    /// -----------------------------------------------------------------------

    /// A current block has been organized (header_t).
    /// Issued by 'transaction' and handled by 'protocol_header/block_out'.
    block,

    /// A confirmable block has been confirmed (header_t).
    /// Issued by 'confirm' [and handled by 'transaction'].
    organized,

    /// A previously confirmed block has been unconfirmed (header_t).
    /// Issued by 'confirm' [and handled by 'transaction'].
    reorganized,

    /// Mining.
    /// -----------------------------------------------------------------------

    /// A transaction has been added to the pool (transaction_t).
    /// Issued by 'transaction' and handled by 'template'.
    transaction,

    /// A candidate block (template) has been created (height_t).
    /// Issued by 'template' and handled by [miners].
    template_,

    /// Stop.
    /// -----------------------------------------------------------------------

    /// Service is stopping, accompanied by error::service_stopped (default).
    stop
};

} // namespace node
} // namespace libbitcoin

#endif
