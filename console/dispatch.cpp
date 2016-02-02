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

namespace libbitcoin {
namespace node {

using boost::format;
using std::placeholders::_1;
using namespace boost::system;
using namespace boost::filesystem;
using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::network;
using namespace bc::wallet;

constexpr auto append = std::ofstream::out | std::ofstream::app;

static void display_version(std::ostream& stream)
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

    output << format(BN_INITCHAIN) % directory << std::endl;

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

// Cheesy command line processor (replace with libbitcoin processor).
static console_result process_arguments(int argc, const char* argv[],
    const path& directory, std::ostream& output, std::ostream& error)
{
    if (argc > 1)
    {
        std::string argument(argv[1]);

        if (argument == "-h" || argument == "--help")
        {
            output << "bn [--help] [--mainnet] [--testnet] [--version]" << std::endl;
            return console_result::not_started;
        }
        else if (argument == "-v" || argument == "--version")
        {
            display_version(output);
            return console_result::not_started;
        }
        else if (argument == "-m" || argument == "--mainnet")
        {
            return init_chain(directory, false, output, error);
        }
        else if (argument == "-t" || argument == "--testnet")
        {
            return init_chain(directory, true, output, error);
        }
        else
        {
            error << "Invalid argument: " << argument << std::endl;
            return console_result::failure;
        }
    }

    return console_result::okay;
}

// Static handler for catching termination signals.
static auto stopped = false;
static void interrupt_handler(int code)
{
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGABRT, interrupt_handler);

    if (code != 0)
        stopped = true;
}

console_result dispatch(int argc, const char* argv[], std::istream& input,
    std::ostream& output, std::ostream& error)
{
    // Config is hardwired for now.
    const auto config = configuration::mainnet;
    const auto directory = config.chain.database_path;

    // Handle command line arguments.
    auto result = process_arguments(argc, argv, directory, output, error);
    if (result != console_result::okay)
        return result;

    // Ensure the blockchain directory is initialized (at least exists).
    result = verify_chain(directory, bc::cerr);
    if (result != console_result::okay)
        return result;

    // These must be libbitcoin streams.
    bc::ofstream debug_file(config.network.debug_file.string(), append);
    bc::ofstream error_file(config.network.error_file.string(), append);
    initialize_logging(debug_file, error_file, bc::cout, bc::cerr);

    static const auto startup = "================= startup ==================";
    log::debug(LOG_NODE) << startup;
    log::info(LOG_NODE) << startup;
    log::warning(LOG_NODE) << startup;
    log::error(LOG_NODE) << startup;
    log::fatal(LOG_NODE) << startup;

    log::info(LOG_NODE) << format(BN_NODE_STARTING) % directory;

    p2p_node node(config);

    // The stop handlers are registered in start.
    node.start(std::bind(handle_started, _1, std::ref(node)));

    return wait_for_stop(node);
}

// This is called at the end of seeding.
void handle_started(const code& ec, p2p_node& node)
{
    if (ec)
    {
        log::info(LOG_NODE) << BN_NODE_START_FAIL;
        stopped = true;
        return;
    }

    // Start running the node (header and block sync for now).
    node.run(std::bind(handle_running, _1));
}

// This is called at the end of block sync, though execution continues after.
void handle_running(const code& ec)
{
    if (ec)
    {
        log::info(LOG_NODE) << BN_NODE_START_FAIL;
        stopped = true;
        return;
    }

    // The service is running now, waiting on us to call stop.
}

console_result wait_for_stop(p2p_node& node)
{
    // Set up the stop handler.
    std::promise<code> promise;
    const auto stop_handler = [&promise](code ec) { promise.set_value(ec); };

    // Monitor stopped for completion.
    monitor_for_stop(node, stop_handler);

    // Block until the stop handler is invoked.
    const auto result = promise.get_future().get();
    return result ? console_result::failure : console_result::okay;
}

void monitor_for_stop(p2p_node& node, p2p::result_handler handler)
{
    using namespace std::chrono;
    using namespace std::this_thread;
    std::string command;

    interrupt_handler(0);
    log::info(LOG_NODE) << BN_NODE_START_SUCCESS;

    while (!stopped)
        sleep_for(milliseconds(10));

    log::info(LOG_NODE) << BN_NODE_SHUTTING_DOWN;
    node.stop(handler);
}

} // namespace node
} // namespace libbitcoin
