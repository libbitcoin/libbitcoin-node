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
#include "executive.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

using boost::format;
using std::placeholders::_1;
using namespace boost::system;
using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::network;

static constexpr int no_interrupt = 0;
static constexpr int directory_exists = 0;
static constexpr int directory_not_found = 2;
static constexpr auto append = std::ofstream::out | std::ofstream::app;

static const auto application_name = "bn";
static const auto stop_sensitivity = std::chrono::milliseconds(10);

executive::executive(parser& metadata, std::istream& input,
    std::ostream& output, std::ostream& error)
  : metadata_(metadata),
    input_(input), output_(output), error_(error),
    debug_file_(metadata_.configuration.network.debug_file.string(), append),
    error_file_(metadata_.configuration.network.error_file.string(), append)
{
    initialize_logging(debug_file_, error_file_, output_, error_);

    static const auto startup = "================= startup ==================";
    log::debug(LOG_NODE) << startup;
    log::info(LOG_NODE) << startup;
    log::warning(LOG_NODE) << startup;
    log::error(LOG_NODE) << startup;
    log::fatal(LOG_NODE) << startup;
    log::info(LOG_NODE) << BN_NODE_STARTING;
}

// ----------------------------------------------------------------------------
// Command line options.

void executive::do_help()
{
    const auto options = metadata_.load_options();
    printer help(options, application_name, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
}

void executive::do_settings()
{
    const auto settings = metadata_.load_settings();
    printer print(settings, application_name, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
}

void executive::do_version()
{
    output_ << format(BN_VERSION_MESSAGE) % LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION % LIBBITCOIN_VERSION << std::endl;
}

bool executive::do_initchain()
{
    error_code ec;
    const auto& directory = metadata_.configuration.chain.database_path;

    if (create_directories(directory, ec))
    {
        output_ << format(BN_INITIALIZING_CHAIN) % directory << std::endl;

        // Unfortunately we are still limited to a choice of hardcoded chains.
        const auto genesis = metadata_.configuration.chain.use_testnet_rules ?
            testnet_genesis_block() : mainnet_genesis_block();

        return database::initialize(directory, genesis);
    }

    if (ec.value() == directory_exists)
    {
        error_ << format(BN_INITCHAIN_EXISTS) % directory << std::endl;
        return false;
    }

    error_ << format(BN_INITCHAIN_NEW) % directory % ec.message() << std::endl;
    return false;
}

// ----------------------------------------------------------------------------
// Invoke an action based on command line option.

bool executive::invoke()
{
    const auto config = metadata_.configuration;

    // Show the user the config file in use.
    if (!config.file.empty())
        output_ << format(BN_USING_CONFIG_FILE) % config.file << std::endl;

    if (config.help)
    {
        do_help();
        return true;
    }

    if (config.settings)
    {
        do_settings();
        return true;
    }

    if (config.version)
    {
        do_version();
        return true;
    }

    if (config.initchain)
    {
        return do_initchain();
    }

    // There are no command line arguments, just run the node.
    return run();
}

// ----------------------------------------------------------------------------
// Run sequence.

bool executive::run()
{
    // Ensure the blockchain directory is initialized (at least exists).
    if (!verify())
        return false;

    // Now that the directory is verified we can create the node for it.
    node_ = std::make_shared<p2p_node>(metadata_.configuration);

    // Start seeding the node, stop handlers are registered in start.
    node_->start(
        std::bind(&executive::handle_started,
            shared_from_this(), _1));

    // Block until the node is stopped or there is an interrupt.
    return wait_on_stop();
}

// Use missing directory as a sentinel indicating lack of initialization.
bool executive::verify()
{
    error_code ec;
    const auto& directory = metadata_.configuration.chain.database_path;

    if (exists(directory, ec))
        return true;

    if (ec.value() == directory_not_found)
    {
        error_ << format(BN_UNINITIALIZED_CHAIN) % directory << std::endl;
        return false;
    }

    error_ << format(BN_INITCHAIN_TRY) % directory % ec.message() << std::endl;
    return false;
}

// Static handler for catching termination signals.
static auto stopped_ = false;
static void interrupt_handler(int code)
{
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGABRT, interrupt_handler);

    if (code != no_interrupt && !stopped_)
    {
        bc::cout << format(BN_NODE_STOPPING) % code << std::endl;
        stopped_ = true;
    }
}

// This is called at the end of seeding.
void executive::handle_started(const code& ec)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stopped_ = true;
        return;
    }

    // Start running the node (header and block sync for now).
    node_->run(
        std::bind(&executive::handle_running,
            shared_from_this(), _1));
}

// This is called at ???, though execution continues after.
void executive::handle_running(const code& ec)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stopped_ = true;
        return;
    }

    // The node is running now, waiting on node|interrupt stop.
}

void executive::handle_stopped(const code& ec, std::promise<code>& promise)
{
    promise.set_value(ec);
}

bool executive::wait_on_stop()
{
    std::promise<code> promise;

    // Monitor stopped for completion.
    monitor_stop(
        std::bind(&executive::handle_stopped,
            shared_from_this(), _1, std::ref(promise)));

    // Block until the stop handler is invoked.
    const auto ec = promise.get_future().get();

    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_STOP_FAIL) % ec.message();
        return false;
    }

    log::info(LOG_NODE) << BN_NODE_STOPPED;
    return true;
}

void executive::monitor_stop(p2p::result_handler handler)
{
    interrupt_handler(no_interrupt);
    log::info(LOG_NODE) << BN_NODE_STARTED;

    while (!stopped_ && !node_->stopped())
        std::this_thread::sleep_for(stop_sensitivity);

    log::info(LOG_NODE) << BN_NODE_UNMAPPING;
    node_->stop(handler);
    node_->close();
}

} // namespace node
} // namespace libbitcoin
