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
#include <bitcoin/node/protocol_block_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/reservation.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block_sync"
#define CLASS protocol_block_sync

using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

// The interval in which block download rate is tested.
static const asio::seconds expiry_interval(5);

protocol_block_sync::protocol_block_sync(p2p& network, channel::ptr channel,
    reservation::ptr row)
  : protocol_timer(network, channel, true, NAME),
    reservation_(row),
    CONSTRUCT_TRACK(protocol_block_sync)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start(event_handler handler)
{
    auto complete = synchronize(BIND2(blocks_complete, _1, handler), 1, NAME);
    protocol_timer::start(expiry_interval, BIND2(handle_event, _1, complete));

    SUBSCRIBE3(block, handle_receive, _1, _2, complete);

    // This is the end of the start sequence.
    send_get_blocks(complete, true);
}

// Peer sync sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::send_get_blocks(event_handler complete, bool reset)
{
    if (stopped())
        return;

    // If the channel has been drained of hashes we are done.
    if (reservation_->empty())
    {
        log::info(LOG_PROTOCOL)
            << "Stopping complete slot (" << reservation_->slot() << ").";
        complete(error::success);
        return;
    }

    // We may have a new set of hashes to request.
    const auto packet = reservation_->request(reset);

    // Or the hashes may have already been requested.
    if (packet.inventories.empty())
        return;

    log::debug(LOG_PROTOCOL)
        << "Sending request of " << packet.inventories.size()
        << " hashes for slot (" << reservation_->slot() << ").";

    SEND2(packet, handle_send, _1, complete);
}

void protocol_block_sync::handle_send(const code& ec, event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::warning(LOG_PROTOCOL)
            << "Failure sending request to slot (" << reservation_->slot()
            << ") " << ec.message();
        complete(ec);
    }
}

// The message subscriber implements an optimization to bypass queueing of
// block messages. This requires that this handler never call back into the
// subscriber. Otherwise a deadlock will result. This in turn requires that
// the 'complete' parameter handler never call into the message subscriber.
bool protocol_block_sync::handle_receive(const code& ec, block::ptr message,
    event_handler complete)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Receive failure on slot (" << reservation_->slot() << ") "
            << ec.message();
        complete(ec);
        return false;
    }

    // Add the block to the blockchain store.
    reservation_->import(message);

    if (reservation_->partitioned())
    {
        log::info(LOG_PROTOCOL)
            << "Restarting partitioned slot (" << reservation_->slot() << ").";
        complete(error::channel_stopped);
        return false;
    }

    // Request more blocks if our reservation has been expanded.
    send_get_blocks(complete, false);
    return true;
}

// This is fired by the base timer and stop handler.
void protocol_block_sync::handle_event(const code& ec, event_handler complete)
{
    if (ec == error::channel_stopped)
    {
        complete(ec);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        log::info(LOG_PROTOCOL)
            << "Failure in block sync timer for slot (" << reservation_->slot()
            << ") " << ec.message();
        complete(ec);
        return;
    }

    if (reservation_->expired())
    {
        log::info(LOG_PROTOCOL)
            << "Restarting slow slot (" << reservation_->slot() << ")";
        complete(error::channel_timeout);
        return;
    }

    // Do not return sucess here, we need to make sure the race for new hashes
    // doesn't result in a segment of hashes getting dropped by success here.
    if (reservation_->empty())
    {
        log::debug(LOG_PROTOCOL)
            << "Reservation is empty (" << reservation_->slot() << ") "
            << ec.message();
        complete(error::channel_timeout);
    }
}

void protocol_block_sync::blocks_complete(const code& ec,
    event_handler handler)
{
    reservation_->set_idle();

    // This is the end of the peer sync sequence.
    handler(ec);

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
