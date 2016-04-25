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
#include <bitcoin/node/session_block_sync.hpp>

#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/hash_queue.hpp>
#include <bitcoin/node/protocol_block_sync.hpp>
#include <bitcoin/node/reservation.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_block_sync
#define NAME "session_block_sync"

using namespace blockchain;
using namespace config;
using namespace network;
using std::placeholders::_1;
using std::placeholders::_2;

// The interval in which all-channel block download performance is tested.
static const asio::seconds regulator_interval(5);

session_block_sync::session_block_sync(p2p& network, hash_queue& hashes,
    block_chain& chain, const settings& settings)
  : session_batch(network, false),
    blockchain_(chain),
    settings_(settings),
    reservations_(hashes, chain, settings),
    CONSTRUCT_TRACK(session_block_sync)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void session_block_sync::start(result_handler handler)
{
    // TODO: create session_timer base class and pass interval via start.
    timer_ = std::make_shared<deadline>(pool_, regulator_interval);
    session::start(CONCURRENT2(handle_started, _1, handler));
}

void session_block_sync::handle_started(const code& ec, result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    // Copy the reservations table.
    const auto table = reservations_.table();

    if (table.empty())
    {
        handler(error::success);
        return;
    }

    const auto connector = create_connector();
    const auto complete = synchronize(handler, table.size(), NAME);

    // This is the end of the start sequence.
    for (const auto row: table)
        new_connection(connector, row, complete);

    ////reset_timer(connector);
}

// Block sync sequence.
// ----------------------------------------------------------------------------

void session_block_sync::new_connection(connector::ptr connect,
    reservation::ptr row, result_handler handler)
{
    if (stopped())
    {
        log::debug(LOG_SESSION)
            << "Suspending slot (" << row ->slot() << ").";
        return;
    }

    log::debug(LOG_SESSION)
        << "Starting slot (" << row->slot() << ").";

    // BLOCK SYNC CONNECT
    this->connect(connect,
        BIND5(handle_connect, _1, _2, connect, row, handler));
}

void session_block_sync::handle_connect(const code& ec, channel::ptr channel,
    connector::ptr connect, reservation::ptr row, result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_SESSION)
            << "Failure connecting slot (" << row->slot() << ") "
            << ec.message();
        new_connection(connect, row, handler);
        return;
    }

    log::debug(LOG_SESSION)
        << "Connected slot (" << row->slot() << ") ["
        << channel->authority() << "]";

    register_channel(channel,
        BIND5(handle_channel_start, _1, channel, connect, row, handler),
        BIND2(handle_channel_stop, _1, row));
}

void session_block_sync::handle_channel_start(const code& ec,
    channel::ptr channel, connector::ptr connect, reservation::ptr row,
    result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, connect, row, handler);
        return;
    }

    attach<protocol_ping>(channel)->start();
    attach<protocol_address>(channel)->start();
    attach<protocol_block_sync>(channel, row)->start(
        BIND4(handle_complete, _1, connect, row, handler));
};

void session_block_sync::handle_complete(const code& ec,
    network::connector::ptr connect, reservation::ptr row,
    result_handler handler)
{
    if (!ec)
    {
        timer_->stop();
        reservations_.remove(row);

        log::debug(LOG_SESSION)
            << "Completed slot (" << row->slot() << ")";

        // This is the end of the block sync sequence.
        handler(ec);
        return;
    }

    // There is no failure scenario, we ignore the result code here.
    new_connection(connect, row, handler);
}

void session_block_sync::handle_channel_stop(const code& ec,
    reservation::ptr row)
{
    log::info(LOG_SESSION)
        << "Channel stopped on slot (" << row->slot() << ") " << ec.message();
}

// Timer.
// ----------------------------------------------------------------------------

// private:
void session_block_sync::reset_timer(connector::ptr connect)
{
    if (stopped())
        return;

    timer_->start(BIND2(handle_timer, _1, connect));
}

void session_block_sync::handle_timer(const code& ec, connector::ptr connect)
{
    if (stopped())
        return;

    log::debug(LOG_PROTOCOL)
        << "Fired session_block_sync timer: " << ec.message();

    ////// TODO: If (ratio < 50%) add a channel.
    ////// TODO: push into reservations_ iplementation.
    ////// TODO: add a boolean increment method to the synchronizer and pass here.
    ////const size_t id = reservations_.table().size();
    ////const auto row = std::make_shared<reservation>(reservations_, id);
    ////const synchronizer<result_handler> handler({}, 0, "name", true);

    ////if (true)
    ////    new_connection(connect, row, handler);

    ////// TODO: If (ratio - (1/row_count) > 50%) drop the slowest channel.
    ////// reservations_.prune();

    reset_timer(connect);
}

} // namespace node
} // namespace libbitcoin
