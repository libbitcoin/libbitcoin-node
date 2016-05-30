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
#ifndef LIBBITCOIN_NODE_EXECUTOR_HPP
#define LIBBITCOIN_NODE_EXECUTOR_HPP

#include <future>
#include <iostream>
#include <memory>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

class executor
  : public enable_shared_from_base<executor>
{
public:
    typedef std::shared_ptr<executor> ptr;

    executor(parser& metadata, std::istream&, std::ostream& output,
        std::ostream& error);

    /// This class is not copyable.
    executor(const executor&) = delete;
    void operator=(const executor&) = delete;

    /// Invoke the menu command indicated by the metadata.
    bool menu();

private:
    static void stop(const code& ec);
    static void handle_stop(int code);

    void do_help();
    void do_settings();
    void do_version();
    bool do_initchain();

    bool start();
    void initialize_output();
    bool verify_directory();
    void monitor_stop();

    void handle_started(const code& ec);
    void handle_running(const code& ec);
    void handle_stopped(const code& ec);

    // Termination state.
    static std::promise<code> stopped_;

    parser& metadata_;
    std::ostream& output_;
    bc::ofstream debug_file_;
    bc::ofstream error_file_;
    p2p_node::ptr node_;
};
    
// Localizable messages.
#define BN_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BN_INFORMATION_MESSAGE \
    "Runs a full bitcoin node with additional client-server query protocol."

#define BN_UNINITIALIZED_CHAIN \
    "The %1% directory is not initialized."
#define BN_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BN_INITCHAIN_NEW \
    "Failed to create directory %1% with error, '%2%'."
#define BN_INITCHAIN_EXISTS \
    "Failed because the directory %1% already exists."
#define BN_INITCHAIN_TRY \
    "Failed to test directory %1% with error, '%2%'."

#define BN_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BN_NODE_STARTING \
    "Please wait while the node is starting..."
#define BN_NODE_START_FAIL \
    "Node failed to start with error, %1%."
#define BN_NODE_SEEDED \
    "Seeding is complete."
#define BN_NODE_STARTED \
    "Node is started."

#define BN_NODE_STOPPING \
    "Please wait while the node is stopping (code: %1%)..."
#define BN_NODE_UNMAPPING \
    "Please wait while files are unmapped..."
#define BN_NODE_STOPPED \
    "Node stopped successfully."

#define BN_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BN_USING_DEFAULT_CONFIG \
    "Using default configuration settings."
#define BN_VERSION_MESSAGE \
    "\nVersion Information:\n\n" \
    "libbitcoin-node:       %1%\n" \
    "libbitcoin-blockchain: %2%\n" \
    "libbitcoin:            %3%"
#define BN_LOG_HEADER \
    "================= startup =================="

} // namespace node
} // namespace libbitcoin

#endif