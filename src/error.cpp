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
    { unknown, "unknown error" },
    { unexpected_event, "unexpected event" },

    // database
    { store_integrity, "store integrity" },
    { store_uninitialized, "store not initialized" },

    // network
    { slow_channel, "slow channel" },
    { stalled_channel, "stalled channel" },

    // blockchain
    { orphan_header, "orphan header" },
    { insufficient_work, "insufficient work" },
    { duplicate_block, "duplicate block" }
};

DEFINE_ERROR_T_CATEGORY(error, "node", "node code")

} // namespace error
} // namespace database
} // namespace libbitcoin
