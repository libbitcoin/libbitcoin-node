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

namespace libbitcoin {
namespace node {

using boost::format;
using namespace bc::system;
using namespace bc::system::config;
using namespace bc::database;
using namespace bc::network;
using namespace std::placeholders;

const char* executor::name = "bn";
std::promise<code> executor::stopping_;

// TODO: parameterize sink from config.
executor::executor(parser& metadata, std::istream&, std::ostream& output,
    std::ostream& error) NOEXCEPT
  : metadata_(metadata),
    store_(metadata.configured.database),
    query_(store_),
    output_(output),
    error_(error),
    sink_{ "log1.txt", "log2.txt", 10 * 1024 }
{
    initialize_stop();
}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu() NOEXCEPT
{
    const auto& config = metadata_.configured;

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

// Command line options.
// ----------------------------------------------------------------------------

// --help
void executor::do_help() NOEXCEPT
{
    const auto options = metadata_.load_options();
    printer help(options, name, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
}

// --settings
// TODO: settings are parsing empty.
void executor::do_settings() NOEXCEPT
{
    const auto settings = metadata_.load_settings();
    printer print(settings, name, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
}

// --version
void executor::do_version() NOEXCEPT
{
    output_ << format(BN_VERSION_MESSAGE) %
        LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION %
        LIBBITCOIN_SYSTEM_VERSION << std::endl;
}

// --initchain
bool executor::do_initchain() NOEXCEPT
{
    const auto& directory = metadata_.configured.database.dir;
    output_ << format(BN_INITIALIZING_CHAIN) % directory << std::endl;

    if (!file::create_directory(directory))
    {
        output_ << format(BN_INITCHAIN_EXISTS) % directory << std::endl;
        return false;
    }

    if (const auto ec = store_.create())
    {
        output_ << format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message()
            << std::endl;
        return false;
    }

    if (const auto ec = store_.open())
    {
        output_ << format(BN_INITCHAIN_DATABASE_OPEN_FAILURE) % ec.message()
            << std::endl;
        return false;
    }

    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        output_ << BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE << std::endl;
        return false;
    }

    if (const auto ec = store_.close())
    {
        output_ << format(BN_INITCHAIN_DATABASE_CLOSE_FAILURE) % ec.message()
            << std::endl;
        return false;
    }

    output_ << BN_INITCHAIN_COMPLETE << std::endl;
    return true;
}

// Run.
// ----------------------------------------------------------------------------

// TODO: hosts file not created on first run until close.
bool executor::run() NOEXCEPT
{
    sink_.start();
    log_.subscribe([&](const code&, const std::string& message) NOEXCEPT
    {
        sink_.write(message);
        ////sink_.flush();
    });
    log_.subscribe([&](const code&, const std::string& message) NOEXCEPT
    {
        output_ << message;
        output_.flush();
    });

    log_.write() << format(BN_LOG_HEADER) % local_time() << std::endl;

    const auto& file = metadata_.configured.file;

    if (file.empty())
        log_.write() << BN_USING_DEFAULT_CONFIG << std::endl;
    else
        log_.write() << format(BN_USING_CONFIG_FILE) % file << std::endl;

    // Create and start node.
    log_.write() << BN_NODE_INTERRUPT << std::endl;
    log_.write() << BN_NODE_STARTING << std::endl;

    if (const auto ec = store_.open())
    {
        log_.write() << format(BN_STORE_START_FAIL) % ec.message() << std::endl;
        return false;
    }

    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);
    node_->start(std::bind(&executor::handle_started, this, _1));

    // Wait on stop interrupt and then signal node close.
    stopping_.get_future().wait();

    log_.write() << BN_NODE_STOPPING << std::endl;

    node_->close();
    log_.stop(BN_NODE_STOPPED "\n");
    sink_.stop();

    if (const auto ec = store_.close())
    {
        log_.write() << format(BN_STORE_STOP_FAIL) % ec.message() << std::endl;
        return false;
    }

    log_.write() << BN_NODE_STOPPED << std::endl;
    return true; 
}

void executor::handle_started(const code& ec) NOEXCEPT
{
    if (ec)
    {
        if (ec == system::error::not_found)
            log_.write() << format(BN_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.dir << std::endl;
        else
            log_.write() << format(BN_NODE_START_FAIL) % ec.message()
                << std::endl;

        stop(ec);
        return;
    }

    log_.write() << BN_NODE_SEEDED << std::endl;

    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_handler, this, _1));
}

void executor::handle_handler(const code& ec) NOEXCEPT
{
    if (ec)
    {
        log_.write() << format(BN_NODE_START_FAIL) % ec.message() << std::endl;
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec) NOEXCEPT
{
    if (ec)
    {
        log_.write() << format(BN_NODE_START_FAIL) % ec.message() << std::endl;
        stop(ec);
        return;
    }

    log_.write() << BN_NODE_STARTED << std::endl;
}

void executor::handle_stopped(const code& ec) NOEXCEPT
{
    if (ec && ec != network::error::service_stopped)
        log_.write() << format(BN_NODE_STOP_CODE) % ec.message() << std::endl;

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
    stop(system::error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec) NOEXCEPT
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&]() NOEXCEPT { stopping_.set_value(ec); });
}

} // namespace node
} // namespace libbitcoin
