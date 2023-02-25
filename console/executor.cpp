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
#define LOGGER(message) log_.write(level_t::news) << message << std::endl

// TODO: look into boost signal_set.
// www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/overview/signals.html

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace system;
using namespace network;
using namespace std::placeholders;

const char* executor::name = "bn";
std::promise<code> executor::stopping_;

executor::executor(parser& metadata, std::istream& input, std::ostream& output,
    std::ostream&)
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

    return run();
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
    CONSOLE(format(BN_VERSION_MESSAGE) % LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION % LIBBITCOIN_SYSTEM_VERSION);
    return true;
}

// --initchain
bool executor::do_initchain()
{
    log_.stop();
    const auto& directory = metadata_.configured.database.path;
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

bool executor::run()
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

    log_.subscribe_events([&](const code& ec, uint8_t event, size_t count,
        const logger::time_point& point) NOEXCEPT
        {
            if (ec) return false;
            sink_
                << encode_base16(to_big_endian(point.time_since_epoch().count()))
                << " [" << serialize(event) << "." << count << "]" << std::endl;
            return true;
    });

    // Handlers are invoked on a strand of the logger threadpool.
    if (metadata_.configured.light)
    {
        log_.subscribe_messages(
            [&](const code& ec, uint8_t level, time_t time,
                const std::string& message) NOEXCEPT
            {
                if (!ec && (level == level_t::quit || level == level_t::proxy))
                    return true;

                const auto prefix = format_zulu_time(time) + "." +
                    serialize(level) + " ";

                if (ec)
                {
                    sink_ << prefix << message << std::endl;
                    sink_ << prefix << BN_NODE_FOOTER << std::endl;
                    log_stopped_.set_value(ec);
                    return false;
                }
                else
                {
                    sink_ << prefix << message;
                    return true;
                }
            });

        LOGGER(BN_LOG_HEADER);
    }
    else
    {
        log_.subscribe_messages(
            [&](const code& ec, uint8_t level, time_t time,
                const std::string& message) NOEXCEPT
            {
                if (!ec && (level == level_t::quit || level == level_t::proxy))
                    return true;

                const auto prefix = format_zulu_time(time) + "." +
                    serialize(level) + " ";

                if (ec)
                {
                    sink_ << prefix << message << std::endl;
                    output_ << prefix << message << std::endl;
                    sink_ << prefix << BN_NODE_FOOTER << std::endl;
                    output_ << prefix << BN_NODE_FOOTER << std::endl;
                    log_stopped_.set_value(ec);
                    return false;
                }
                else
                {
                    sink_ << prefix << message;
                    output_ << prefix << message;
                    output_.flush();
                    return true;
                }
            });

        LOGGER(BN_LOG_HEADER);
    }

    const auto& file = metadata_.configured.file;
    if (file.empty())
    {
        LOGGER(BN_USING_DEFAULT_CONFIG);
    }
    else
    {
        LOGGER(format(BN_USING_CONFIG_FILE) % file);
    }

    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        LOGGER(format(BN_UNINITIALIZED_STORE) % store);
        log_.stop(BN_NODE_STOPPED);
        log_stopped_.get_future().wait();
        return false;
    }

    // Open store, create and start node, wait on stop interrupt.
    LOGGER(BN_NODE_INTERRUPT);
    LOGGER(BN_NODE_STARTING);

    if (const auto ec = store_.open())
    {
        LOGGER(format(BN_STORE_START_FAIL) % ec.message());
        log_.stop(BN_NODE_STOPPED);
        log_stopped_.get_future().wait();
        return false;
    }

    // initialize network settings.
    metadata_.configured.network.initialize();

    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);
    node_->subscribe_connect(
        [&](const code&, const channel::ptr&)
        {
            if (to_bool(metadata_.configured.node.interval) &&
                is_zero(node_->channel_count() %
                    metadata_.configured.node.interval))
            {
                LOGGER(
                    "{in:" << node_->inbound_channel_count() << "}"
                    "{ch:" << node_->channel_count() << "}"
                    "{rv:" << node_->reserved_count() << "}"
                    "{nc:" << node_->nonces_count() << "}"
                    "{ad:" << node_->address_count() << "}"
                    "{bs:" << node_->broadcast_count() << "}"
                    "{ss:" << node_->stop_subscriber_count() << "}"
                    "{cs:" << node_->connect_subscriber_count() << "}.");
            }

            if (to_bool(metadata_.configured.node.target) &&
                (node_->channel_count() >= metadata_.configured.node.target))
            {
                LOGGER("Stopping at channel target ("
                    << metadata_.configured.node.target << ").");

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

    node_->subscribe_close(
        [&](const code&)
        {
            LOGGER(
                "{in:" << node_->inbound_channel_count() << "}"
                "{ch:" << node_->channel_count() << "}"
                "{rv:" << node_->reserved_count() << "}"
                "{nc:" << node_->nonces_count() << "}"
                "{ad:" << node_->address_count() << "}"
                "{bs:" << node_->broadcast_count() << "}"
                "{ss:" << node_->stop_subscriber_count() << "}"
                "{cs:" << node_->connect_subscriber_count() << "}.");
            return false;
        },
        [&](const code&, size_t)
        {
            // By not handling it is possible stop could fire before complete.
            // But the handler is not required for termination, so this is ok.
            // The error code in the handler can be used to differentiate.
        });

    LOGGER("Log period: " << metadata_.configured.node.interval);
    LOGGER("Stop target: " << metadata_.configured.node.target);

    node_->start(std::bind(&executor::handle_started, this, _1));
    stopping_.get_future().wait();

    // Close node, close store, stop logger, stop log sink.
    LOGGER(BN_NODE_STOPPING);
    node_->close();

    if (const auto ec = store_.close())
    {
        LOGGER(format(BN_STORE_STOP_FAIL) % ec.message());
        log_.stop(BN_NODE_STOPPED);
        log_stopped_.get_future().wait();
        return false;
    }

    log_.stop(BN_NODE_STOPPED);
    log_stopped_.get_future().wait();
    return true; 
}

void executor::handle_started(const code& ec)
{
    if (ec)
    {
        if (ec == error::store_uninitialized)
        {
            LOGGER(format(BN_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.path);
        }
        else
        {
            LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        }

        stop(ec);
        return;
    }

    LOGGER(BN_NODE_STARTED);

    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_subscribed, this, _1, _2));
}

void executor::handle_subscribed(const code& ec, size_t)
{
    if (ec)
    {
        LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        LOGGER(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    LOGGER(BN_NODE_RUNNING);
}

bool executor::handle_stopped(const code& ec)
{
    if (ec && ec != network::error::service_stopped)
    {
        LOGGER(format(BN_NODE_STOP_CODE) % ec.message());
    }

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
