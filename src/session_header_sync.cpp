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
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/hash_queue.hpp>
#include <bitcoin/node/protocol_header_sync.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_header_sync

using namespace config;
using namespace network;
using std::placeholders::_1;
using std::placeholders::_2;

// The starting minimum header download rate, exponentially backs off.
static constexpr uint32_t headers_per_second = 10000;

// Sort is required here but not in configuration settings.
session_header_sync::session_header_sync(p2p& network, hash_queue& hashes,
    const settings& settings, const blockchain::settings& chain_settings)
  : session_batch(network, false),
    hashes_(hashes),
    settings_(settings),
    minimum_rate_(headers_per_second),
    checkpoints_(sort(chain_settings.checkpoints)),
    CONSTRUCT_TRACK(session_header_sync)
{
}

// Checkpoints are not sorted in config but must be here.
checkpoint::list session_header_sync::sort(checkpoint::list checkpoints)
{
    return checkpoint::sort(checkpoints);
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

    // The hash list must be seeded with at least one trusted hash.
    if (hashes_.empty())
    {
        handler(error::operation_failed);
        return;
    }

    // Sync up to the last checkpoint or trusted entry only.
    if (checkpoints_.empty() ||
        hashes_.last_height() >= checkpoints_.back().height())
    {
        handler(error::success);
        return;
    }

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
        log::debug(LOG_SESSION)
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
        log::debug(LOG_SESSION)
            << "Failure connecting header sync channel: " << ec.message();
        new_connection(connect, handler);
        return;
    }

    log::debug(LOG_NETWORK)
        << "Connected to header sync channel [" << channel->authority() << "]";

    register_channel(channel,
        BIND4(handle_channel_start, _1, connect, channel, handler),
        BIND1(handle_channel_stop, _1));
}

void session_header_sync::handle_channel_start(const code& ec,
    connector::ptr connect, channel::ptr channel, result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, connect, handler);
        return;
    }

    attach<protocol_ping>(channel)->start();
    attach<protocol_address>(channel)->start();
    attach<protocol_header_sync>(channel, hashes_, minimum_rate_, checkpoints_)
        ->start(BIND3(handle_complete, _1, connect, handler));
};

void session_header_sync::handle_complete(const code& ec,
    network::connector::ptr connect, result_handler handler)
{
    if (!ec)
    {
        // This is the end of the header sync sequence.
        handler(ec);
        return;
    }

    // Reduce the rate minimum so that we don't get hung up.
    minimum_rate_ /= 2;

    // There is no failure scenario, we ignore the result code here.
    new_connection(connect, handler);
}

void session_header_sync::handle_channel_stop(const code& ec)
{
    log::debug(LOG_SESSION)
        << "Header sync channel stopped: " << ec.message();
}

} // namespace node
} // namespace libbitcoin
