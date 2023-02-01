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
#include "executor.hpp"

#include <csignal>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <boost/core/null_deleter.hpp>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

#define CONSOLE(message) output_ << message << std::endl
#define LOGGER(message) log_.write() << message << std::endl

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace std::placeholders;

const char* executor::name = "bn";
std::promise<code> executor::stopping_;

executor::executor(parser& metadata, std::istream& input, std::ostream& output,
    std::ostream&) NOEXCEPT
  : metadata_(metadata),
    store_(metadata.configured.database),
    query_(store_),
    input_(input),
    output_(output)
{
    initialize_stop();
}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu() NOEXCEPT
{
    const auto& config = metadata_.configured;

    if (config.help)
        return do_help();

    if (config.settings)
        return do_settings();

    if (config.version)
        return do_version();

    if (config.initchain)
        return do_initchain();

    return run();
}

// Command line options.
// ----------------------------------------------------------------------------

// --help
bool executor::do_help() NOEXCEPT
{
    printer help(metadata_.load_options(), name, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
    return true;
}

// --settings
bool executor::do_settings() NOEXCEPT
{
    printer print(metadata_.load_settings(), name, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
    return true;
}

// --version
bool executor::do_version() NOEXCEPT
{
    CONSOLE(format(BN_VERSION_MESSAGE) % LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION % LIBBITCOIN_SYSTEM_VERSION);
    return true;
}

// --initchain
bool executor::do_initchain() NOEXCEPT
{
    const auto& directory = metadata_.configured.database.dir;
    CONSOLE(format(BN_INITIALIZING_CHAIN) % directory);

    if (!database::file::create_directory(directory))
    {
        CONSOLE(format(BN_INITCHAIN_EXISTS) % directory);
        return false;
    }

    if (const auto ec = store_.create())
    {
        CONSOLE(format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message());
        return false;
    }

    if (const auto ec = store_.open())
    {
        CONSOLE(format(BN_INITCHAIN_DATABASE_OPEN_FAILURE) % ec.message());
        return false;
    }

    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        CONSOLE(BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        return false;
    }

    if (const auto ec = store_.close())
    {
        CONSOLE(format(BN_INITCHAIN_DATABASE_CLOSE_FAILURE) % ec.message());
        return false;
    }

    CONSOLE(BN_INITCHAIN_COMPLETE);
    return true;
}

// Run.
// ----------------------------------------------------------------------------

bool executor::run() NOEXCEPT
{
    // No logging for log setup, could use console.
    const auto& logs = metadata_.configured.log.path;
    if (!logs.empty())
        database::file::create_directory(logs);

    database::file::stream::out::rotator sink_
    {
        // Standard file names, both within the logs directory.
        metadata_.configured.log.file1(),
        metadata_.configured.log.file2(),
        to_half(metadata_.configured.log.maximum_size)
    };

    // Handlers are invoked on a strand of the logger threadpool.
    if (metadata_.configured.light)
        log_.subscribe([&](const code&, const std::string& message) NOEXCEPT
        {
            sink_ << message;
        });
    else
        log_.subscribe([&](const code&, const std::string& message) NOEXCEPT
        {
            sink_ << message;
            output_ << message;
            output_.flush();
        });

    LOGGER(format(BN_LOG_HEADER) % network::local_time());

    const auto& file = metadata_.configured.file;
    if (file.empty())
    {
        LOGGER(BN_USING_DEFAULT_CONFIG);
    }
    else
    {
        LOGGER(format(BN_USING_CONFIG_FILE) % file);
    }

    const auto& store = metadata_.configured.database.dir;
    if (!database::file::is_directory(store))
    {
        LOGGER(format(BN_UNINITIALIZED_STORE) % store);
        log_.stop(BN_NODE_STOPPED "\n");
        sink_.flush();
        return false;
    }

    // Open store, create and start node, wait on stop interrupt.
    LOGGER(BN_NODE_INTERRUPT);
    LOGGER(BN_NODE_STARTING);

    if (const auto ec = store_.open())
    {
        LOGGER(format(BN_STORE_START_FAIL) % ec.message());
        log_.stop(BN_NODE_STOPPED "\n");
        sink_.flush();
        return false;
    }

    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);
    node_->start(std::bind(&executor::handle_started, this, _1));
    stopping_.get_future().wait();

    // Close node, close store, stop logger, stop log sink.
    LOGGER(BN_NODE_STOPPING);
    node_->close();

    if (const auto ec = store_.close())
    {
        LOGGER(format(BN_STORE_STOP_FAIL) % ec.message());
        log_.stop(BN_NODE_STOPPED "\n");
        sink_.flush();
        return false;
    }

    log_.stop(BN_NODE_STOPPED "\n");
    sink_.flush();
    return true; 
}

void executor::handle_started(const code& ec) NOEXCEPT
{
    if (ec)
    {
        if (ec == error::store_uninitialized)
        {
            LOGGER(format(BN_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.dir);
        }
        else
        {
            LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        }

        stop(ec);
        return;
    }

    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_subscribed, this, _1));
}

void executor::handle_subscribed(const code& ec) NOEXCEPT
{
    if (ec)
    {
        LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec) NOEXCEPT
{
    if (ec)
    {
        LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    LOGGER(BN_NODE_STARTED);
}

void executor::handle_stopped(const code& ec) NOEXCEPT
{
    if (ec && ec != network::error::service_stopped)
    {
        LOGGER(format(BN_NODE_STOP_CODE) % ec.message());
    }

    stop(ec);
}

// Stop signal.
// ----------------------------------------------------------------------------

void executor::initialize_stop() NOEXCEPT
{
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);
}

void executor::handle_stop(int) NOEXCEPT
{
    initialize_stop();
    stop(error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec) NOEXCEPT
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&]() NOEXCEPT { stopping_.set_value(ec); });
}

} // namespace node
} // namespace libbitcoin
