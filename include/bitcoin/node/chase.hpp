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
#ifndef LIBBITCOIN_NODE_CHASE_HPP
#define LIBBITCOIN_NODE_CHASE_HPP

#include <bitcoin/node/events.hpp>

namespace libbitcoin {
namespace node {

enum class chase
{
    /// A new candidate branch exists (height_t).
    /// Issued by 'header' and handled by 'check'.
    header,

    /// New candidate headers without txs exist (count_t).
    /// Issued by 'check' and handled by 'block_in_31800'.
    download,

    /// Channel is starved for block download identifiers (channel_t).
    /// Issued by 'block_in_31800' and handled by 'session_outbound'.
    starved,

    /// Channel (slow) is directed to split its work and stop (channel_t).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    split,

    /// Channels (all with work) are directed to split work and stop (0).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    stall,

    /// Channels (all with work) are directed to drop work and stop (0).
    /// Issued by 'check' and handled by 'block_in_31800'.
    purge,

    /// Channels (all) are directed to pause reading.
    /// Issued by 'full_node' and handled by 'protocol'.
    pause,

    /// Channels (all) are directed to resume reading.
    /// Issued by 'full_node' and handled by 'protocol'.
    resume,

    /// Chaser is directed to start validating (height_t).
    /// Issued by 'full_node' and handled by 'preconfirm'.
    bump,

    /// A block has been downloaded, checked and stored (height_t).
    /// Issued by 'block_in_31800' and handled by 'connect'.
    checked,

    /// A downloaded block has failed check (header_t).
    /// Issued by 'block_in_31800' and handled by 'header'.
    unchecked,

    /// A branch has been preconfirmed (header_t).
    /// Issued by 'preconfirm' and handled by 'confirm'.
    preconfirmed,

    /// A checked block has failed preconfirm (header_t).
    /// Issued by 'preconfirm' and handled by 'header'.
    unpreconfirmed,

    /// A branch has been confirmed (header_t).
    /// Issued by 'confirm' and handled by 'transaction'.
    confirmed,

    /// A connected block has failed confirm (header_t).
    /// Issued by 'confirm' and handled by 'header' (and 'block').
    unconfirmed,

    /// unchecked, unpreconfirmed or unconfirmed was handled (height_t).
    /// Issued by 'organize' and handled by 'preconfirm' and 'confirm'.
    disorganized,

    /// A new transaction has been added to the pool (transaction_t).
    /// Issued by 'transaction' and handled by 'candidate'.
    transaction,

    /// A new candidate block (template) has been created ().
    /// Issued by 'candidate' and handled by [miners].
    candidate,

    /// Legacy: A new strong branch exists (branch height_t).
    /// Issued by 'block' and handled by 'confirm'.
    block,

    /// Service is stopping (accompanied by error::service_stopped), ().
    stop
};

} // namespace node
} // namespace libbitcoin

#endif
