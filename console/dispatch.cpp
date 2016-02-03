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
#include "dispatch.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

#define BN_APPLICATION_NAME "bn"

namespace libbitcoin {
namespace node {

using boost::format;
using std::placeholders::_1;
using namespace boost::system;
using namespace boost::filesystem;
using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::network;

constexpr auto append = std::ofstream::out | std::ofstream::app;

static void display_invalid_parameter(std::ostream& stream,
    const std::string& message)
{
    // English-only hack to patch missing arg name in boost exception message.
    std::string clean_message(message);
    boost::replace_all(clean_message, "for option is invalid", "is invalid");
    stream << format(BN_INVALID_PARAMETER) % clean_message << std::endl;
}

static void show_help(parser& metadata, std::ostream& stream)
{
    printer help(metadata.load_options(), metadata.load_arguments(),
        BN_APPLICATION_NAME, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(stream);
}

static void show_settings(parser& metadata, std::ostream& stream)
{
    printer print(metadata.load_settings(), BN_APPLICATION_NAME,
        BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(stream);
}

static void show_version(std::ostream& stream)
{
    stream << format(BN_VERSION_MESSAGE) %
        LIBBITCOIN_NODE_VERSION % 
        LIBBITCOIN_BLOCKCHAIN_VERSION % 
        LIBBITCOIN_VERSION << std::endl;
}

// Create the directory as a convenience for the user, and then use it
// as sentinel to guard against inadvertent re-initialization.
static console_result init_chain(const path& directory, bool testnet,
    std::ostream& output, std::ostream& error)
{
    error_code ec;
    if (!create_directories(directory, ec))
    {
        if (ec.value() == 0)
            error << format(BN_INITCHAIN_DIR_EXISTS) % directory << std::endl;
        else
            error << format(BN_INITCHAIN_DIR_NEW) % directory % ec.message()
                << std::endl;

        return console_result::failure;
    }

    output << format(BN_INITIALIZING_CHAIN) % directory << std::endl;

    const auto prefix = directory.string();
    const auto genesis = testnet ? testnet_genesis_block() :
        mainnet_genesis_block();

    return database::initialize(prefix, genesis) ?
        console_result::not_started : console_result::failure;
}

// Use missing directory as a sentinel indicating lack of initialization.
static console_result verify_chain(const path& directory, std::ostream& error)
{
    error_code ec;
    if (!exists(directory, ec))
    {
        if (ec.value() == 2)
            error << format(BN_UNINITIALIZED_CHAIN) % directory << std::endl;
        else
            error << format(BN_INITCHAIN_DIR_TEST) % directory % ec.message()
                << std::endl;

        return console_result::failure;
    }

    return console_result::okay;
}

static console_result run(const configuration& configuration,
    std::ostream& output, std::ostream& error)
{
    // This must be verified before node/blockchain construct.
    // Ensure the blockchain directory is initialized (at least exists).
    const auto result = verify_chain(configuration.chain.database_path, error);
    if (result != console_result::okay)
        return result;

    // TODO: make member of new dispatch class.
    p2p_node node(configuration);

    // TODO: initialize on construct of new dispatch class.
    // These must be libbitcoin streams.
    bc::ofstream debug_file(configuration.network.debug_file.string(), append);
    bc::ofstream error_file(configuration.network.error_file.string(), append);
    initialize_logging(debug_file, error_file, bc::cout, bc::cerr);

    static const auto startup = "================= startup ==================";
    log::debug(LOG_NODE) << startup;
    log::info(LOG_NODE) << startup;
    log::warning(LOG_NODE) << startup;
    log::error(LOG_NODE) << startup;
    log::fatal(LOG_NODE) << startup;
    log::info(LOG_NODE) << BN_NODE_STARTING;

    // The stop handlers are registered in start.
    node.start(std::bind(handle_started, _1, std::ref(node)));

    // Block until the node is stopped.
    return wait_for_stop(node);
}

// Load argument, environment and config and then run the node.
console_result dispatch(int argc, const char* argv[], std::istream&,
    std::ostream& output, std::ostream& error)
{
    parser metadata;
    std::string error_message;

    if (!metadata.parse(error_message, argc, argv))
    {
        display_invalid_parameter(error, error_message);
        return console_result::failure;
    }

    const auto settings = metadata.settings;
    if (!settings.file.empty())
        output << format(BN_USING_CONFIG_FILE) % settings.file << std::endl;

    if (settings.help)
        show_help(metadata, output);
    else if (settings.settings)
        show_settings(metadata, output);
    else if (settings.version)
        show_version(output);
    else if (settings.main_network)
        return init_chain(settings.chain.database_path, false, output, error);
    else if (settings.test_network)
        return init_chain(settings.chain.database_path, true, output, error);
    else
        return run(settings, output, error);

    return console_result::okay;
}

// Static handler for catching termination signals.
static auto stopped = false;
static void interrupt_handler(int code)
{
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGABRT, interrupt_handler);

    if (code != 0 && !stopped)
    {
        bc::cout << format(BN_NODE_STOPPING) % code << std::endl;
        stopped = true;
    }
}

// TODO: use node as member.
// This is called at the end of seeding.
void handle_started(const code& ec, p2p_node& node)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stopped = true;
        return;
    }

    // Start running the node (header and block sync for now).
    node.run(std::bind(handle_running, _1, std::ref(node)));
}

// TODO: use node as member.
// This is called at the end of block sync, though execution continues after.
void handle_running(const code& ec, p2p_node&)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stopped = true;
        return;
    }

    // The node is running now, waiting on stopped to be set to true.

    ///////////////////////////////////////////////////////////////////////////
    // ATTACH ADDITIONAL SERVICES HERE
    ///////////////////////////////////////////////////////////////////////////
}

// TODO: use node as member.
console_result wait_for_stop(p2p_node& node)
{
    // Set up the stop handler.
    std::promise<code> promise;
    const auto stop_handler = [&promise](code ec) { promise.set_value(ec); };

    // Monitor stopped for completion.
    monitor_for_stop(node, stop_handler);

    // Block until the stop handler is invoked.
    const auto ec = promise.get_future().get();

    if (ec)
    {
        log::info(LOG_NODE) << format(BN_NODE_STOP_FAIL) % ec.message();
        return console_result::failure;
    }

    log::info(LOG_NODE) << BN_NODE_STOPPED;
    return console_result::okay;
}

// TODO: use node as member.
void monitor_for_stop(p2p_node& node, p2p::result_handler handler)
{
    using namespace std::chrono;
    using namespace std::this_thread;
    std::string command;

    interrupt_handler(0);
    log::info(LOG_NODE) << BN_NODE_STARTED;

    while (!stopped)
        sleep_for(milliseconds(10));

    log::info(LOG_NODE) << BN_NODE_UNMAPPING;
    node.stop(handler);
}

} // namespace node
} // namespace libbitcoin
