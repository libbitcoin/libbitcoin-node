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
#include <bitcoin/node/config/endpoint.hpp>
#include <bitcoin/node/config/checkpoint.hpp>

namespace libbitcoin {
namespace system {
    
// TODO: move into libbitcoin-system.
class BCN_API settings
{
public:
    uint32_t network_threads;
    uint16_t inbound_port;
    uint32_t inbound_connection_limit;
    uint32_t outbound_connections;
    uint32_t connect_timeout_seconds;
    uint32_t channel_expiration_minutes;
    uint32_t channel_timeout_minutes;
    uint32_t channel_heartbeat_minutes;
    uint32_t channel_startup_minutes;
    uint32_t channel_revivial_minutes;
    uint32_t host_pool_capacity;
    boost::filesystem::path hosts_file;
    boost::filesystem::path debug_file;
    boost::filesystem::path error_file;
    std::vector<node::endpoint_type> seeds;
};

} // namespace system

// TODO: move into libbitcoin-blockchain.
namespace chain {

class BCN_API settings
{
public:
    uint32_t blockchain_threads;
    uint32_t block_pool_capacity;
    uint32_t history_start_height;
    boost::filesystem::path database_path;
    std::vector<node::checkpoint_type> checkpoints;
};

} // namespace chain

namespace node {

class BCN_API settings
{
public:
    uint32_t node_threads;
    uint32_t transaction_pool_capacity;
    std::vector<endpoint_type> peers;
    std::vector<endpoint_type> bans;
};

class BCN_API settings_type
{
public:
    node::settings node;
    chain::settings chain;
    system::settings system;

    // HACK: remove once logging is fully injected.
    std::string skip_log;
};

} // namespace node
} // namespace libbitcoin

#endif
