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
#ifndef LIBBITCOIN_NODE_EVENTS_HPP
#define LIBBITCOIN_NODE_EVENTS_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

/// Reporting events.
enum events : uint8_t
{
    event_archive,
    event_header,
    event_block,
    event_validated,
    event_confirmed,
    event_current_headers,
    event_current_blocks,
    event_current_validated,
    event_current_confirmed
};

}
}

#endif
