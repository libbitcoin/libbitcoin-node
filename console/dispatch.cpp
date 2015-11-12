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

#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

// Localizable messages.
#define BN_FETCH_HISTORY_SUCCESS \
    "Fetched history for [%1%]\n"
#define BN_FETCH_HISTORY_FAIL \
    "Fetch history failed for [%1%] : %2%\n"
#define BN_FETCH_HISTORY_INPUT \
    "Input [%1%] : %2% %3% %4%\n"
#define BN_FETCH_HISTORY_OUTPUT \
    "Output [%1%] : %2% %3% %4%\n"
#define BN_FETCH_HISTORY_SPEND \
    "Spend : %1%\n"
#define BN_INVALID_ADDRESS \
    "Invalid address."
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
    "Type a bitcoin address to fetch, or 'stop' to stop node."
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

using boost::format;
using namespace boost::system;
using namespace boost::filesystem;
using namespace bc;
using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::node;
using namespace bc::network;
using namespace bc::wallet;

constexpr auto append = std::ofstream::out | std::ofstream::app;

static void display_history(const std::error_code& ec,
    const block_chain::history& history,
    const wallet::payment_address& address, std::ostream& output)
{
    if (ec)
    {
        output << format(BN_FETCH_HISTORY_FAIL) % address.encoded() %
            ec.message();
        return;
    }

    output << format(BN_FETCH_HISTORY_SUCCESS) % address.encoded();

    for (const auto& row: history)
    {
        const auto hash = bc::encode_hash(row.point.hash);
        if (row.kind == block_chain::point_kind::output)
            output << format(BN_FETCH_HISTORY_OUTPUT) % hash %
                row.point.index % row.height % row.value;
        else
            output << format(BN_FETCH_HISTORY_INPUT) % hash %
                row.point.index % row.height % row.value;
    }
}

static void display_version(std::ostream& stream)
{
    stream << format(BN_VERSION_MESSAGE) % LIBBITCOIN_NODE_VERSION % 
        LIBBITCOIN_BLOCKCHAIN_VERSION % LIBBITCOIN_VERSION << std::endl;
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

console_result dispatch(int argc, const char* argv[], std::istream& input,
    std::ostream& output, std::ostream& error)
{
    // Config is hardwired for now.
    const auto config = p2p_node::defaults;
    const auto directory = config.chain.database_path;

    // Handle command line arguments.
    auto result = process_arguments(argc, argv, directory, output, error);
    if (result != console_result::okay)
        return result;

    // Ensure the blockchain directory is initialized (at least exists).
    result = verify_chain(directory, bc::cerr);
    if (result != console_result::okay)
        return result;

    // Suppress abort so it's picked up in the loop by getline.
    const auto interrupt_handler = [](int) {};
    signal(SIGABRT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGINT, interrupt_handler);

    output << format(BN_NODE_STARTING) % directory << std::endl;

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

    p2p_node node(config);
    std::promise<code> start_promise;
    const auto handle_start = [&start_promise, &node](const code ec)
    {
        start_promise.set_value(ec);
    };

    // Start the node.
    node.start(handle_start);
    auto ec = start_promise.get_future().get();

    if (ec)
    {
        output << BN_NODE_START_FAIL << std::endl;
        return console_result::not_started;
    }

    std::promise<code>run_promise;
    const auto handle_run = [&run_promise](const code ec)
    {
        run_promise.set_value(ec);
    };

    // Start the long-running sessions.
    node.run(handle_run);
    ec = run_promise.get_future().get();

    while (true)
    {
        std::string command;
        std::getline(bc::cin, command);
        const auto trimmed = boost::trim_copy(command);
        if (command == "\0x03" || trimmed == "stop")
        {
            output << BN_NODE_SHUTTING_DOWN << std::endl;
            break;
        }
    }

    std::promise<code> stop_promise;
    const auto handle_stop = [&stop_promise](const code ec)
    {
        stop_promise.set_value(ec);
    };

    // Stop the service.
    node.stop(handle_stop);
    ec = stop_promise.get_future().get();

    if (ec)
    {
        output << BN_NODE_STOP_FAIL << std::endl;
        return console_result::failure;
    }

    return console_result::okay;
}
