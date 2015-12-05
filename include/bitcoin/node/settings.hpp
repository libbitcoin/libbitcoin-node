/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_SETTINGS_HPP
#define LIBBITCOIN_NODE_SETTINGS_HPP

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
    
/// default settings
#define NODE_THREADS                        4
#define NODE_QUORUM                         8
#define NODE_BLOCKS_PER_SECOND              16
#define NODE_HEADERS_PER_SECOND             10000
#define NODE_TRANSACTION_POOL_CAPACITY      10000
#define NODE_PEERS                          config::endpoint::list()

struct BCN_API settings
{
    uint32_t threads;
    uint32_t quorum;
    uint32_t blocks_per_second;
    uint32_t headers_per_second;
    uint32_t transaction_pool_capacity;
    config::endpoint::list peers;
};

} // namespace node
} // namespace libbitcoin

#endif
