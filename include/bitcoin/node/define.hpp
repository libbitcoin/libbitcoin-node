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
#ifndef LIBBITCOIN_NODE_DEFINE_HPP
#define LIBBITCOIN_NODE_DEFINE_HPP

/// Standard includes (do not include directly).
#include <functional>
#include <memory>
#include <utility>
#include <variant>

/// Pulls in common /node headers (excluding settings/config/parser/full_node).
#include <bitcoin/node/chase.hpp>

/// Now we use the generic helper definitions above to define BCN_API
/// and BCN_INTERNAL. BCN_API is used for the public API symbols. It either DLL
/// imports or DLL exports (or does nothing for static build) BCN_INTERNAL is
/// used for non-api symbols.
#if defined BCN_STATIC
    #define BCN_API
    #define BCN_INTERNAL
#elif defined BCN_DLL
    #define BCN_API      BC_HELPER_DLL_EXPORT
    #define BCN_INTERNAL BC_HELPER_DLL_LOCAL
#else
    #define BCN_API      BC_HELPER_DLL_IMPORT
    #define BCN_INTERNAL BC_HELPER_DLL_LOCAL
#endif

/// For common types below.
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

/// Alias system code.
/// TODO: std::error_code "node" category holds node::error::error_t.
typedef std::error_code code;

/// Organization types.
typedef std::function<void(const code&, size_t)> organize_handler;
typedef database::store<database::map> store;
typedef database::query<store> query;

/// Hash accumulator.
typedef std::shared_ptr<database::associations> map_ptr;
typedef std::function<void(const code&, const map_ptr&)> map_handler;

/// Node events.
typedef std::variant<uint32_t, uint64_t> event_link;
typedef network::subscriber<chase, event_link> event_subscriber;
typedef event_subscriber::handler event_handler;

/// Use for event_link variants.
using channel_t = uint64_t;
using header_t = database::header_link::integer;
using transaction_t = database::tx_link::integer;
////using count_t = size_t;  // quantity
////using header_t = size_t; // height

} // namespace node
} // namespace libbitcoin

#endif

// define.hpp is the common include for /node.
// All non-node headers include define.hpp.
// Node inclusions are chained as follows.

// version        : <generated>
// error          : version
// events         : error
// chase          : events
// define         : chase

// Other directory common includes are not internally chained.
// Each header includes only its required common headers.

// settings       : define
// configuration  : define settings
// parser         : define configuration
// /chasers       : define configuration  [forward: full_node]
// full_node      : define /chasers
// session        : define full_node
// /protocols     : define session

// Session is only included by full_node.cpp (avoids cycle).
// /sessions      : define full_node /protocols