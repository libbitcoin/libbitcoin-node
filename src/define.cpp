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
#include <bitcoin/node/define.hpp>

// define.hpp is the common include for /node.
// All non-node headers include define.hpp.
// Node inclusions are chained as follows.

// version        : <generated>
// error          : version <bitcoin/system>
// events         : error
// chase          : events <bitcoin/database>
// define         : chase <bitcoin/network>

// Other directory common includes are not internally chained.
// Each header includes only its required common headers.

// block_arena    : define
// block_memory   : block_arena
// settings       : define
// configuration  : settings
// estimator      : define
// /chasers       : configuration estimator        [forward: full_node]
// /channels      : configuration
// /protocols     : /channels /chasers             [sessions/session.hpp]
// /sessions      : /protocols                     [forward: full_node]
// full_node      : /sessions

// When a symbol is unexplainably undefined, its defining include is probably
// creating the circularity.
