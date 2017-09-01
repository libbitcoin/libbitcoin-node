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
#include <future>
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

// The moving window in which block average download rate is measured.
static const asio::seconds expiry_interval(5);

// Depends on protocol_header_sync, which requires protocol version 31800.
protocol_block_sync::protocol_block_sync(full_node& network,
    channel::ptr channel, safe_chain& chain)
  : protocol_timer(network, channel, true, NAME),
    chain_(chain),
    reservation_(network.get_reservation()),
    CONSTRUCT_TRACK(protocol_block_sync)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start()
{
    protocol_timer::start(expiry_interval, BIND1(handle_event, _1));

    chain_.subscribe_headers(BIND4(handle_reindexed, _1, _2, _3, _4));
    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    // This is the end of the start sequence.
    send_get_blocks();
}

// Download sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::send_get_blocks()
{
    if (stopped())
        return;

    // Don't start downloading blocks until the header chain is current.
    if (chain_.is_headers_stale())
        return;

    // Repopulate if empty and new work has arrived.
    const auto request = reservation_->request();

    // Or we may be the same channel and with hashes already requested.
    if (request.inventories().empty())
        return;

    LOG_DEBUG(LOG_NODE)
        << "Sending request of " << request.inventories().size()
        << " hashes for slot (" << reservation_->slot() << ").";

    SEND2(request, handle_send, _1, request.command);
}

bool protocol_block_sync::handle_receive_block(const code& ec,
    block_const_ptr message)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        stop(ec);
        return false;
    }

    // TODO: make import a sequential call and fan out within the blockchain.
    std::promise<code> complete;

    // Add the block's transactions to the store.
    reservation_->import(chain_, message,
        BIND3(handle_import_block, _1, message, std::ref(complete)));

    // This prevents multiple blocks from queuing up behind the store.
    return !complete.get_future().get();
}

void protocol_block_sync::handle_import_block(const code& ec,
    block_const_ptr message, std::promise<code>& complete)
{
    if (stopped(ec))
    {
        complete.set_value(ec);
        return;
    }

    if (ec)
    {
        stop(ec);
        complete.set_value(ec);
        return;
    }

    // This channel is slow and half of its reservation has been taken.
    if (reservation_->toggle_partitioned())
    {
        LOG_DEBUG(LOG_NODE)
            << "Restarting partitioned slot (" << reservation_->slot() << ").";

        stop(error::channel_stopped);
        complete.set_value(error::channel_stopped);
        return;
    }

    send_get_blocks();
    complete.set_value(error::success);
}

// Events.
// ----------------------------------------------------------------------------

// Use header indexation as a block request trigger.
bool protocol_block_sync::handle_reindexed(code ec, size_t,
    header_const_ptr_list_const_ptr, header_const_ptr_list_const_ptr)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        stop(ec);
        return false;
    }

    send_get_blocks();
    return true;
}

// This is fired by base timer and stop handler, used for speed evaluation.
void protocol_block_sync::handle_event(const code& ec)
{
    if (stopped(ec))
    {
        // We are no longer receiving blocks, so free up the reservation.
        reservation_->stop();
        return;
    }

    if (ec == error::channel_timeout && reservation_->expired())
    {
        LOG_DEBUG(LOG_NODE)
            << "Restarting slow slot (" << reservation_->slot() << ")";
        stop(error::channel_timeout);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        LOG_DEBUG(LOG_NODE)
            << "Failure in block sync timer for slot (" << reservation_->slot()
            << ") " << ec.message();
        stop(ec);
        return;
    }
}

} // namespace node
} // namespace libbitcoin
