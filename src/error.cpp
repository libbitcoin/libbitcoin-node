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
    { orphan_block, "orphan block" },
    { orphan_header, "orphan header" },
    { duplicate_block, "duplicate block" },
    { duplicate_header, "duplicate header" },

    /// faults
    { protocol1, "protocol1" },
    { protocol2, "protocol2" },
    { header1, "header1" },
    { organize1, "organize1" },
    { organize2, "organize2" },
    { organize3, "organize3" },
    { organize4, "organize4" },
    { organize5, "organize5" },
    { organize6, "organize6" },
    { organize7, "organize7" },
    { organize8, "organize8" },
    { organize9, "organize9" },
    { organize10, "organize10" },
    { organize11, "organize11" },
    { organize12, "organize12" },
    { organize13, "organize13" },
    { organize14, "organize14" },
    { organize15, "organize15" },
    { validate1, "validate1" },
    { validate2, "validate2" },
    { validate3, "validate3" },
    { validate4, "validate4" },
    { validate5, "validate5" },
    { validate6, "validate6" },
    { validate7, "validate7" },
    ////{ validate8, "validate8" },
    ////{ validate9, "validate9" },
    { confirm1, "confirm1" },
    { confirm2, "confirm2" },
    { confirm3, "confirm3" },
    { confirm4, "confirm4" },
    { confirm5, "confirm5" },
    { confirm6, "confirm6" },
    { confirm7, "confirm7" },
    { confirm8, "confirm8" },
    { confirm9, "confirm9" },
    { confirm10, "confirm10" },
    { confirm11, "confirm11" },
    { confirm12, "confirm12" },
    { confirm13, "confirm13" }
    ////{ confirm14, "confirm14" },
    ////{ confirm15, "confirm15" }
};

DEFINE_ERROR_T_CATEGORY(error, "node", "node code")

} // namespace error
} // namespace node
} // namespace libbitcoin
