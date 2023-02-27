/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_EXECUTOR_HPP
#define LIBBITCOIN_NODE_EXECUTOR_HPP

#include <atomic>
#include <future>
#include <iostream>
#include <bitcoin/database.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

class executor
{
public:
    DELETE_COPY(executor);

    executor(parser& metadata, std::istream&, std::ostream& output,
        std::ostream& error);

    /// Invoke the menu command indicated by the metadata.
    bool menu();

private:
    void logger(const auto& message);
    void console(const auto& message);
    void stopper(const auto& message);

    static void initialize_stop();
    static void stop(const system::code& ec);
    static void handle_stop(int code);

    void handle_started(const system::code& ec);
    void handle_subscribed(const system::code& ec, size_t key);
    void handle_running(const system::code& ec);
    bool handle_stopped(const system::code& ec);

    bool do_help();
    bool do_settings();
    bool do_version();
    bool do_initchain();
    bool do_run();

    using rotator_t = database::file::stream::out::rotator;
    rotator_t create_sink(const std::filesystem::path& path) const;
    void subscribe_light(rotator_t& sink);
    void subscribe_full(rotator_t& sink);
    void subscribe_events(rotator_t& sink);
    void subscribe_capture();
    void subscribe_connect();
    void subscribe_close();

    static const char* name;
    static std::promise<system::code> stopping_; 
    std::atomic_bool log_objects_{ false };
    std::promise<system::code> stopped_{};

    full_node::ptr node_{};
    parser& metadata_;
    store_t store_;
    query_t query_;

    std::istream& input_;
    std::ostream& output_;
    network::logger log_{};
    network::capture cap_{ input_, "q" };
};

// Localizable messages (set at level_t::reserved).

#define BN_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BN_INFORMATION_MESSAGE \
    "Runs a full bitcoin node with additional client-server query protocol."

#define BN_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BN_INITCHAIN_EXISTS \
    "Failed because the directory %1% already exists."
#define BN_INITCHAIN_COMPLETE \
    "Completed initialization."
#define BN_INITCHAIN_DATABASE_CREATE_FAILURE \
    "Database creation failed with error, '%1%'."
#define BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE \
    "Database failure to store genesis block."
#define BN_INITCHAIN_DATABASE_OPEN_FAILURE \
    "Database failure to open, %1%."
#define BN_INITCHAIN_DATABASE_CLOSE_FAILURE \
    "Database failure to close, %1%."

#define BN_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BN_NODE_STARTING \
    "Please wait while the node is starting..."
#define BN_NODE_START_FAIL \
    "Node failed to start with error, %1%."
#define BN_NODE_STARTED \
    "Node is started."
#define BN_NODE_RUNNING \
    "Node is running."

#define BN_UNINITIALIZED_STORE \
    "The %1% store directory does not exist, run: bn --initchain"
#define BN_UNINITIALIZED_CHAIN \
    "The %1% store is not initialized, delete and run: bn --initchain"
#define BN_STORE_START_FAIL \
    "Store failed to start with error, %1%."
#define BN_STORE_STOP_FAIL \
    "Store failed to stop with error, %1%."

#define BN_NODE_STOPPING \
    "Please wait while the node is stopping..."
#define BN_NODE_STOP_CODE \
    "Node stopped with code, %1%."
#define BN_NODE_STOPPED \
    "Node stopped successfully."
#define BN_CHANNEL_LOG_PERIOD \
    "Log period: %1%"
#define BN_CHANNEL_STOP_TARGET \
    "Stop target: %1%"

#define BN_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BN_USING_DEFAULT_CONFIG \
    "Using default configuration settings."
#define BN_VERSION_MESSAGE \
    "\nVersion Information\n" \
    "----------------------------\n" \
    "libbitcoin-node:       %1%\n" \
    "libbitcoin-blockchain: %2%\n" \
    "libbitcoin-database:   %3%\n" \
    "libbitcoin-network:    %4%\n" \
    "libbitcoin-system:     %5%"
#define BN_LOG_HEADER \
    "====================== startup ======================="
#define BN_NODE_FOOTER \
    "====================== shutdown ======================"
#define BN_NODE_TERMINATE \
    "Press <enter> to exit..."
} // namespace node
} // namespace libbitcoin

#endif
