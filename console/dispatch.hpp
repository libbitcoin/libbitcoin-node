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
#ifndef LIBBITCOIN_NODE_DISPATCH_HPP
#define LIBBITCOIN_NODE_DISPATCH_HPP

#include <iostream>
#include <bitcoin/node.hpp>

// Localizable messages.
#define BN_INITCHAIN \
    "Please wait while initializing %1% directory..."
#define BN_INITCHAIN_DIR_NEW \
    "Failed to create directory %1% with error, '%2%'."
#define BN_INITCHAIN_DIR_EXISTS \
    "Failed because the directory %1% already exists."
#define BN_INITCHAIN_DIR_TEST \
    "Failed to test directory %1% with error, '%2%'."
#define BN_NODE_SHUTTING_DOWN \
    "The node is stopping..."
#define BN_NODE_START_FAIL \
    "The node failed to start."
#define BN_NODE_STOP_FAIL \
    "The node failed to stop."
#define BN_NODE_START_SUCCESS \
    "The node is starting, type CTRL-C to stop."
#define BN_NODE_STOPPING \
    "Please wait while unmapping %1% directory..."
#define BN_NODE_STARTING \
    "Please wait while mapping %1% directory..."
#define BN_UNINITIALIZED_CHAIN \
    "The %1% directory is not initialized."
#define BN_VERSION_MESSAGE \
    "\nVersion Information:\n\n" \
    "libbitcoin-node:       %1%\n" \
    "libbitcoin-blockchain: %2%\n" \
    "libbitcoin:            %3%"

enum console_result : int
{
    failure = -1,
    okay = 0,
    not_started = 1
};

namespace libbitcoin {
namespace node {
    
console_result wait_for_stop(p2p_node& node);
void monitor_for_stop(p2p_node& node, p2p_node::result_handler);

void handle_started(const code& ec, p2p_node& node);
void handle_running(const code& ec);

// Load arguments and config and then run the node.
console_result dispatch(int argc, const char* argv[], std::istream&,
    std::ostream& output, std::ostream& error);

} // namespace node
} // namespace libbitcoin

#endif