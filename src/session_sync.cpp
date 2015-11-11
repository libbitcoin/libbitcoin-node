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
#include <bitcoin/node/session_sync.hpp>

#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

INITIALIZE_TRACK(bc::node::session_sync);

namespace libbitcoin {
namespace node {

using namespace config;
using namespace network;
using std::placeholders::_1;
using std::placeholders::_2;

session_sync::session_sync(threadpool& pool, p2p& network,
    const network::settings& settings)
  : session(pool, network, settings, false, true),
    CONSTRUCT_TRACK(session_sync, LOG_NETWORK)
{
}

void session_sync::start(const checkpoint& check, result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    session::start();
    checkpoint_ = check;
    const auto connect = create_connector();
    new_connection(connect, handler);
}

void session_sync::new_connection(connector::ptr connect, result_handler handler)
{
    fetch_address(
        dispatch_.ordered_delegate(&session_sync::start_syncing,
            shared_from_base<session_sync>(), _1, _2, connect, handler));
}

void session_sync::start_syncing(const code& ec, const config::authority& sync,
    connector::ptr connect, result_handler handler)
{
    if (stopped())
    {
        handler(error::channel_stopped);
        return;
    }

    log::info(LOG_NETWORK)
        << "Contacting sync [" << sync << "]";

    // SYNCHRONIZE CONNECT
    connect->connect(sync,
        dispatch_.ordered_delegate(&session_sync::handle_connect,
            shared_from_base<session_sync>(), _1, _2, sync, connect, handler));
}

void session_sync::handle_connect(const code& ec, channel::ptr channel,
    const authority& sync, connector::ptr connect, result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure connecting [" << sync << "] sync: "
            << ec.message();
        new_connection(connect, handler);
        return;
    }

    log::info(LOG_NETWORK)
        << "Connected to sync [" << channel->authority() << "]";

    register_channel(channel, 
        std::bind(&session_sync::handle_channel_start,
            shared_from_base<session_sync>(), _1, connect, channel, handler),
        std::bind(&session_sync::handle_channel_stop,
            shared_from_base<session_sync>(), _1, connect, handler));
}

void session_sync::handle_channel_start(const code& ec, connector::ptr connect,
    channel::ptr channel, result_handler handler)
{
    // Treat a start failure just like a stop.
    if (ec)
    {
        handle_channel_stop(ec, connect, handler);
        return;
    }

    // Insufficient peer, just stop and try again.
    if (channel->version().start_height < checkpoint_.height())
    {
        channel->stop(error::channel_stopped);
        return;
    }

    attach<protocol_ping>(channel, settings_);
    attach<protocol_address>(channel, settings_);
    ////attach<protocol_sync_headers>(channel, settings_, handler);
};

void session_sync::handle_channel_stop(const code& ec, connector::ptr connect,
    result_handler handler)
{
    // Are we done or just failed?
    if (ec != error::service_stopped)
        new_connection(connect, handler);
}

} // namespace node
} // namespace libbitcoin
