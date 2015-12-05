/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/session_header_sync.hpp>

#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocol_header_sync.hpp>

INITIALIZE_TRACK(bc::node::session_header_sync);

namespace libbitcoin {
namespace node {

#define CLASS session_header_sync

using namespace config;
using namespace network;
using std::placeholders::_1;
using std::placeholders::_2;

session_header_sync::session_header_sync(threadpool& pool, p2p& network,
    hash_list& hashes, const config::checkpoint& top,
    const configuration& configuration)
  : session_batch(pool, network, configuration.network, false),
    votes_(0),
    hashes_(hashes),
    checkpoints_(configuration.chain.checkpoints),
    start_height_(top.height()),
    configuration_(configuration),
    CONSTRUCT_TRACK(session_header_sync)
{
    // Seed the headers list with the top block, matching start_height.
    hashes_.push_back(top.hash());

    // Checkpoints must be sorted but may not be so in config.
    config::checkpoint::sort(checkpoints_);
}

// Start sequence.
// ----------------------------------------------------------------------------

void session_header_sync::start(result_handler handler)
{
    session::start(CONCURRENT2(handle_started, _1, handler));
}

void session_header_sync::handle_started(const code& ec,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    votes_ = 0;

    // This is the end of the start sequence.
    new_connection(create_connector(), handler);
}

// Header sync sequence.
// ----------------------------------------------------------------------------

void session_header_sync::new_connection(connector::ptr connect,
    result_handler handler)
{
    if (stopped())
    {
        log::debug(LOG_NETWORK)
            << "Suspending header sync session.";
        return;
    }

    // HEADER SYNC CONNECT
    this->connect(connect, BIND4(handle_connect, _1, _2, connect, handler));
}

void session_header_sync::handle_connect(const code& ec, channel::ptr channel,
    connector::ptr connect, result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure connecting header sync: " << ec.message();
        new_connection(connect, handler);
        return;
    }

    log::info(LOG_NETWORK)
        << "Connected to header sync channel [" << channel->authority() << "]";

    register_channel(channel,
        BIND4(handle_channel_start, _1, connect, channel, handler),
        BIND1(handle_channel_stop, _1));
}

void session_header_sync::handle_channel_start(
    const code& ec, connector::ptr connect, channel::ptr channel,
    result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, connect, handler);
        return;
    }

    const auto rate = configuration_.node.headers_per_second;

    attach<protocol_ping>(channel)->start(settings_);
    attach<protocol_address>(channel)->start(settings_);
    attach<protocol_header_sync>(channel, rate, start_height_, hashes_,
        checkpoints_)->start(BIND3(handle_complete, _1, connect, handler));
};

// The handler is passed on to the next start call.
void session_header_sync::handle_complete(const code& ec,
    network::connector::ptr connect, result_handler handler)
{
    if (!ec)
        ++votes_;

    // We require a number successful peer syncs, for maximizing height.
    // They do not have to agree, as this is not conflict resolution.
    if (ec || votes_ < configuration_.node.quorum)
    {
        new_connection(connect, handler);
        return;
    }

    // Remove the seed (top) block hash so we only return new headers.
    hashes_.erase(hashes_.begin());

    // This is the end of the header sync sequence.
    handler(error::success);
}

void session_header_sync::handle_channel_stop(const code& ec)
{
    log::debug(LOG_NETWORK)
        << "Header sync channel stopped: " << ec.message();
}

} // namespace node
} // namespace libbitcoin
