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
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace system;
using namespace network;
using namespace std::placeholders;
const char* executor::name = "bn";
std::promise<code> executor::stopping_{};

executor::executor(parser& metadata, std::istream& input, std::ostream& output,
    std::ostream&)
  : metadata_(metadata),
    store_(metadata.configured.database),
    query_(store_),
    input_(input),
    output_(output)
{
    // Turn of console echoing from std::cin to std:cout.
    system::unset_console_echo();

    // Capture <ctrl-c>.
    initialize_stop();
}

// Utility.
// ----------------------------------------------------------------------------

void executor::logger(const auto& message)
{
    log_.write(level_t::reserved) << message << std::endl;
};

void executor::console(const auto& message)
{
    output_ << message << std::endl;
};

void executor::stopper(const auto& message)
{
    cap_.stop();
    log_.stop(message, level_t::reserved);
    stopped_.get_future().wait();
}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu()
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

    return do_run();
}

// Command line options.
// ----------------------------------------------------------------------------

// --help
bool executor::do_help()
{
    log_.stop();
    printer help(metadata_.load_options(), name, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
    return true;
}

// --settings
bool executor::do_settings()
{
    log_.stop();
    printer print(metadata_.load_settings(), name, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
    return true;
}

// --version
bool executor::do_version()
{
    log_.stop();
    console(format(BN_VERSION_MESSAGE)
        % LIBBITCOIN_NODE_VERSION
        % LIBBITCOIN_BLOCKCHAIN_VERSION
        % LIBBITCOIN_DATABASE_VERSION
        % LIBBITCOIN_NETWORK_VERSION
        % LIBBITCOIN_SYSTEM_VERSION);
    return true;
}

// --initchain
bool executor::do_initchain()
{
    log_.stop();
    const auto& directory = metadata_.configured.database.path;
    console(format(BN_INITIALIZING_CHAIN) % directory);

    if (!database::file::create_directory(directory))
    {
        console(format(BN_INITCHAIN_EXISTS) % directory);
        return false;
    }

    if (const auto ec = store_.create())
    {
        console(format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message());
        return false;
    }

    if (const auto ec = store_.open())
    {
        console(format(BN_INITCHAIN_DATABASE_OPEN_FAILURE) % ec.message());
        return false;
    }

    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        console(BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        return false;
    }

    if (const auto ec = store_.close())
    {
        console(format(BN_INITCHAIN_DATABASE_CLOSE_FAILURE) % ec.message());
        return false;
    }

    console(BN_INITCHAIN_COMPLETE);
    return true;
}

// Run.
// ----------------------------------------------------------------------------

// Create message log rotating file sink.
executor::rotator_t executor::create_sink(
    const std::filesystem::path& path) const
{
    // No logging for log setup failure, could use console.
    if (!path.empty())
        database::file::create_directory(path);

    return
    {
        // Standard file names, both within the logs directory.
        metadata_.configured.log.file1(),
        metadata_.configured.log.file2(),
        to_half(metadata_.configured.log.maximum_size)
    };
}

void executor::subscribe_light(rotator_t& sink)
{
    log_.subscribe_messages([&](const code& ec, uint8_t level, time_t time,
        const std::string& message)
    {
        // --light logs only ec and 'reserved' (bn localized) messages.
        if (!ec && (level != level_t::reserved))
            return true;

        const auto prefix = format_zulu_time(time) + "." +
            serialize(level) + " ";

        if (ec)
        {
            // TODO: no show BN_NODE_TERMINATE on capture/ctrl-c stop.
            sink << prefix << message << std::endl;
            output_ << prefix << message << std::endl;
            sink << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_TERMINATE << std::endl;
            stopped_.set_value(ec);
            return false;
        }
        else
        {
            sink << prefix << message;
            output_ << prefix << message;
            output_.flush();
            return true;
        }
    });
}

void executor::subscribe_full(rotator_t& sink)
{
    log_.subscribe_messages([&](const code& ec, uint8_t level, time_t time,
        const std::string& message)
    {
        // Avoiding 'quit' and 'proxy' messages for now (too verbose).
        if (!ec && (level == level_t::quit || level == level_t::proxy))
            return true;

        // Object logging in !NDEBUG builds can be toggled via stdin.
        if (!ec && (!log_objects_ && level == level_t::objects))
            return true;

        const auto prefix = format_zulu_time(time) + "." +
            serialize(level) + " ";

        if (ec)
        {
            // TODO: no show BN_NODE_TERMINATE on capture/ctrl-c stop.
            sink << prefix << message << std::endl;
            output_ << prefix << message << std::endl;
            sink << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_TERMINATE << std::endl;
            stopped_.set_value(ec);
            return false;
        }
        else
        {
            sink << prefix << message;
            output_ << prefix << message;
            output_.flush();
            return true;
        }
    });
}

void executor::subscribe_events(rotator_t& sink)
{
    log_.subscribe_events([&](const code& ec, uint8_t event, size_t count,
        const logger::time_point& point)
    {
        if (ec) return false;
        sink
            << encode_base16(to_big_endian(point.time_since_epoch().count()))
            << " [" << serialize(event) << "." << count << "]" << std::endl;
        return true;
    });
}

inline std::string to_text(bool value)
{
    return value ? "true" : "false";
}

void executor::subscribe_capture()
{
    cap_.subscribe([&](const code& ec, const std::string& line)
    {
        const auto token = system::trim_copy(line);
        if (token.empty())
            return !ec;

        // Quit application.
        if (token == "q")
        {
            logger("CONSOLE: quit");
            stop(error::success);
            return false;
        }

        // TODO: implement as numeric log level toggle.
        if (token == "o")
        {
            log_objects_ = !log_objects_;
            logger("CONSOLE: objects (" + to_text(log_objects_) + ")");
            return true;
        }

        // Echo by line.
        logger("CONSOLE: " + token);
        return !ec;
    },
    [&](const code&)
    {
        // Continue from here to capture all startup std::cin.
    });
}

void executor::subscribe_connect()
{
    node_->subscribe_connect([&](const code&, const channel::ptr&)
    {
        if (to_bool(metadata_.configured.node.interval) &&
            is_zero(node_->channel_count() %
                metadata_.configured.node.interval))
        {
            log_.write(level_t::reserved) <<
                "{in:" << node_->inbound_channel_count() << "}"
                "{ch:" << node_->channel_count() << "}"
                "{rv:" << node_->reserved_count() << "}"
                "{nc:" << node_->nonces_count() << "}"
                "{ad:" << node_->address_count() << "}"
                "{bs:" << node_->broadcast_count() << "}"
                "{ss:" << node_->stop_subscriber_count() << "}"
                "{cs:" << node_->connect_subscriber_count() << "}."
                << std::endl;
        }

        if (to_bool(metadata_.configured.node.target) &&
            (node_->channel_count() >= metadata_.configured.node.target))
        {
            log_.write(level_t::reserved) << "Stopping at channel target ("
                << metadata_.configured.node.target << ")." << std::endl;

            // Signal stop (simulates <ctrl-c>).
            stop(error::success);
            return false;
        }

        return true;
    },
    [&](const code&, uintptr_t)
    {
        // By not handling it is possible stop could fire before complete.
        // But the handler is not required for termination, so this is ok.
        // The error code in the handler can be used to differentiate.
    });
}

void executor::subscribe_close()
{
    node_->subscribe_close([&](const code&)
    {
        log_.write(level_t::reserved) <<
            "{in:" << node_->inbound_channel_count() << "}"
            "{ch:" << node_->channel_count() << "}"
            "{rv:" << node_->reserved_count() << "}"
            "{nc:" << node_->nonces_count() << "}"
            "{ad:" << node_->address_count() << "}"
            "{bs:" << node_->broadcast_count() << "}"
            "{ss:" << node_->stop_subscriber_count() << "}"
            "{cs:" << node_->connect_subscriber_count() << "}."
            << std::endl;

        return false;
    },
    [&](const code&, size_t)
    {
        // By not handling it is possible stop could fire before complete.
        // But the handler is not required for termination, so this is ok.
        // The error code in the handler can be used to differentiate.
    });
}

// ----------------------------------------------------------------------------

// [--light]
bool executor::do_run()
{
    auto sink = create_sink(metadata_.configured.log.path);

    if (metadata_.configured.light)
        subscribe_light(sink);
    else
        subscribe_full(sink);

    subscribe_events(sink);
    subscribe_capture();
    logger(BN_LOG_HEADER);

    const auto& file = metadata_.configured.file;

    if (file.empty())
        logger(BN_USING_DEFAULT_CONFIG);
    else
        logger(format(BN_USING_CONFIG_FILE) % file);

    // Verify store exists.
    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        logger(format(BN_UNINITIALIZED_STORE) % store);
        stopper(BN_NODE_STOPPED);
        return false;
    }

    logger(BN_NODE_INTERRUPT);
    logger(BN_NODE_STARTING);
    cap_.start();

    // Open store.
    if (const auto ec = store_.open())
    {
        logger(format(BN_STORE_START_FAIL) % ec.message());
        stopper(BN_NODE_STOPPED);
        return false;
    }

    // Create node.
    metadata_.configured.network.initialize();
    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);

    // Subscribe node.
    subscribe_connect();
    subscribe_close();

    logger(format(BN_CHANNEL_LOG_PERIOD) % metadata_.configured.node.interval);
    logger(format(BN_CHANNEL_STOP_TARGET) % metadata_.configured.node.target);

    // Start node.
    node_->start(std::bind(&executor::handle_started, this, _1));

    // Wait on signal to stop node (<ctrl-c>).
    stopping_.get_future().wait();
    logger(BN_NODE_STOPPING);

    // Stop node (if not already stopped by self).
    node_->close();

    // Close store.
    if (const auto ec = store_.close())
    {
        logger(format(BN_STORE_STOP_FAIL) % ec.message());
        stopper(BN_NODE_STOPPED);
        return false;
    }

    stopper(BN_NODE_STOPPED);
    return true; 
}

// ----------------------------------------------------------------------------

void executor::handle_started(const code& ec)
{
    if (ec)
    {
        if (ec == error::store_uninitialized)
            logger(format(BN_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.path);
        else
            logger(format(BN_NODE_START_FAIL) % ec.message());

        stop(ec);
        return;
    }

    logger(BN_NODE_STARTED);

    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_subscribed, this, _1, _2));
}

void executor::handle_subscribed(const code& ec, size_t)
{
    if (ec)
    {
        logger(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        logger(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    logger(BN_NODE_RUNNING);
}

bool executor::handle_stopped(const code& ec)
{
    if (ec && ec != network::error::service_stopped)
        logger(format(BN_NODE_STOP_CODE) % ec.message());

    // Signal stop (simulates <ctrl-c>).
    stop(ec);
    return false;
}

// Stop signal.
// ----------------------------------------------------------------------------

void executor::initialize_stop()
{
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);
}

void executor::handle_stop(int)
{
    initialize_stop();
    stop(error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec)
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&]() { stopping_.set_value(ec); });
}

} // namespace node
} // namespace libbitcoin
