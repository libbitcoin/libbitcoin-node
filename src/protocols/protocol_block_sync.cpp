/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_block_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/utility/reservation.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block_sync"
#define CLASS protocol_block_sync
    
using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

// The minimum amount of block history to move the state from idle.
static constexpr size_t minimum_history = 3;

// The moving window in which block average download rate is measured.
static const asio::seconds monitor_interval(5);

// Depends on protocol_header_sync, which requires protocol version 31800.
protocol_block_sync::protocol_block_sync(full_node& node, channel::ptr channel,
    safe_chain& chain)
    : protocol_timer(node, channel, true, NAME),
    node_(node),
    chain_(chain),
    ////idle_limit_(asio::steady_clock::now() + minimum_history *
    ////    node.node_settings().block_latency()),
    CONSTRUCT_TRACK(protocol_block_sync)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start()
{
    protocol_timer::start(monitor_interval, BIND1(handle_event, _1));

    chain_.subscribe_headers(BIND4(handle_reindexed, _1, _2, _3, _4));
    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    // Break off start thread to prevent message subscriber deadlock.
    // This is the end of the start sequence.
    DISPATCH_CONCURRENT0(send_get_blocks);
}

// Download sequence.
// ----------------------------------------------------------------------------

// protected
reservation::ptr protocol_block_sync::get_reservation()
{
    // Critical Section.
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    if (!reservation_)
    {
        mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        reservation_ = node_.get_reservation();
        //---------------------------------------------------------------------
        mutex_.unlock_and_lock_upgrade();
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    return reservation_;
}

void protocol_block_sync::send_get_blocks()
{
    if (stopped())
        return;

    // Don't start downloading blocks until the header chain is current.
    if (chain_.is_headers_stale())
        return;

    const auto reservation = get_reservation();

    // Repopulate if empty and new work has arrived.
    const auto request = reservation->request();

    // Or we may be the same channel and with hashes already requested.
    if (request.inventories().empty())
        return;

    LOG_DEBUG(LOG_NODE)
        << "Sending request of " << request.inventories().size()
        << " hashes for slot (" << reservation->slot() << ").";

    SEND2(request, handle_send, _1, request.command);
}

bool protocol_block_sync::handle_receive_block(const code& ec,
    block_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto reservation = get_reservation();

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in block receive for slot (" << reservation->slot()
            << ") " << ec.message();
        stop(ec);
        return false;
    }

    // Add the block's transactions to the store.
    const auto code = reservation->import(chain_, message);

    if (code)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in block import for slot (" << reservation->slot()
            << ") " << ec.message();
        stop(code);
        return false;
    }

    // This channel is slow and half of its reservation has been taken.
    if (reservation->toggle_partitioned())
    {
        LOG_DEBUG(LOG_NODE)
            << "Restarting partitioned slot (" << reservation->slot() << ").";
        stop(error::channel_stopped);
        return false;
    }

    send_get_blocks();
    return true;
}

// Events.
// ----------------------------------------------------------------------------

// Use header indexation as a block request trigger.
bool protocol_block_sync::handle_reindexed(code ec, size_t,
    header_const_ptr_list_const_ptr, header_const_ptr_list_const_ptr)
{
    if (stopped(ec))
        return false;

    const auto reservation = get_reservation();

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in header index for slot (" << reservation->slot()
            << ") " << ec.message();
        stop(ec);
        return false;
    }

    send_get_blocks();
    return true;
}

// This is fired by base timer and stop handler, used for speed evaluation.
void protocol_block_sync::handle_event(const code& ec)
{
    const auto reservation = get_reservation();

    if (stopped(ec))
    {
        LOG_INFO(LOG_NODE)
            << "stopped: " << reservation->slot();

        // We are no longer receiving blocks, so free up the reservation.
        reservation->stop();

        // Trigger unsubscribe or protocol will hang until next header indexed.
        chain_.unsubscribe();
        return;
    }

    if (ec == error::channel_timeout)
    {
        if (!reservation->idle() && reservation->expired())
        {
            LOG_INFO(LOG_NODE)
                << "Restarting slow slot (" << reservation->slot()
                << ") : [" << reservation->size() << "]";
            stop(ec);
            return;
        }

        // TODO: limit to pending requests.
        // TODO: handle inside of expired() call.
        ////if (reservation->idle() && asio::steady_clock::now() > idle_limit_)
        ////{
        ////    LOG_INFO(LOG_NODE)
        ////        << "Restarting idling slot (" << reservation->slot()
        ////        << ") : [" << reservation->size() << "]";
        ////    stop(ec);
        ////    return;
        ////}
    }
    else if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in block sync timer for slot (" << reservation->slot()
            << ") " << ec.message();
        stop(ec);
    }
}

} // namespace node
} // namespace libbitcoin
