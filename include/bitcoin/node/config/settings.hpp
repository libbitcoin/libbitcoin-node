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
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/config/btc256.hpp>

namespace libbitcoin {
namespace node {

class BCN_API settings
{
public:
    uint16_t database_threads;
    uint16_t network_threads;
    uint16_t memory_threads;

    uint32_t host_pool_capacity;
    uint32_t block_pool_capacity;
    uint32_t tx_pool_capacity;

    uint32_t history_height;
    uint32_t checkpoint_height;
    btc256 checkpoint_hash;

    uint16_t p2p_listen_port;
    uint32_t p2p_outbound_connections;

    boost::filesystem::path p2p_hosts_file;
    boost::filesystem::path blockchain_path;
};

} // namespace node
} // namespace libbitcoin

#endif
