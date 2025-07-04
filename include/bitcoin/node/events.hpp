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
#ifndef LIBBITCOIN_NODE_EVENTS_HPP
#define LIBBITCOIN_NODE_EVENTS_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

/// Reporting events.
enum events : uint8_t
{
    /// Candidate chain.
    header_archived,     // header checked, accepted
    header_organized,    // header pushed (previously archived)
    header_reorganized,  // header popped

    /// Blocks.
    block_archived,      // block checked
    block_buffered,      // block buffered for validation
    block_validated,     // block checked, accepted, connected
    block_confirmed,     // block checked, accepted, connected, confirmable
    block_unconfirmable, // block invalid (after headers-first archive)
    validate_bypassed,   // block checked, accepted [assumed]
    confirm_bypassed,    // block checked, accepted, connected [assumed]

    /// Transactions.
    tx_archived,         // unassociated tx checked, accepted, connected
    tx_validated,        // associated tx checked, accepted, connected
    tx_invalidated,      // associated tx invalid (after headers-first archive)

    /// Confirmed chain.
    block_organized,     // block pushed (previously confirmable)
    block_reorganized,   // block popped

    /// Mining.
    template_issued,      // block template issued for mining

    /// Timespans.
    snapshot_secs,        // snapshot timespan in seconds.
    prune_msecs,          // prune timespan in milliseconds.
    reload_msecs,         // store reload timespan in milliseconds.
    block_msecs,          // getblock timespan in milliseconds.
    ancestry_msecs,       // getancestry timespan in milliseconds.
    filter_msecs,         // getfilter timespan in milliseconds.
    filterhashes_msecs,   // getfilterhashes timespan in milliseconds.
    filterchecks_msecs    // getcfcheckpt timespan in milliseconds.
};

} // namespace node
} // namespace libbitcoin

#endif
