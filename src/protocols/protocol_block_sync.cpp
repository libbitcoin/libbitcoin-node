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

// The moving window in which block average download rate is measured.
static const asio::seconds monitor_interval(5);

// Depends on protocol_header_sync, which requires protocol version 31800.
protocol_block_sync::protocol_block_sync(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_timer(node, channel, true, NAME),
    chain_(chain),
    reservation_(node.get_reservation()),
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
    // This protects against disk fill and allows hashes to be distributed.
    if (chain_.is_candidates_stale())
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
        LOG_ERROR(LOG_NODE)
            << "Failure in block receive for slot (" << reservation_->slot()
            << ") " << ec.message();
        stop(ec);
        return false;
    }

    // This channel was slowest, so half of its reservation has been taken.
    if (reservation_->stopped())
    {
        LOG_DEBUG(LOG_NODE)
            << "Restarting partitioned slot (" << reservation_->slot()
            << ") : [" << reservation_->size() << "]";
        stop(error::channel_stopped);
        return false;
    }

    size_t height;

    // The reservation may have become stopped between the stop test and this
    // call, so the block may either be unrequested or moved to another slot.
    // There is currently no way to know the difference, so log both options.
    if (!reservation_->find_height_and_erase(message->hash(), height))
    {
        LOG_DEBUG(LOG_NODE)
            << "Unrequested or partitioned block on slot ("
            << reservation_->slot() << ").";
        stop(error::channel_stopped);
        return false;
    }

    // Add the block's transactions to the store.
    // If this is the validation target then validator advances here.
    // Block validation failure will not cause an error here.
    // If any block fails validation then reindexation will be triggered.
    // Successful block validation with sufficient height triggers block reorg.
    // However the reorgnization notification cannot be sent from here.
    const auto error_code = reservation_->import(chain_, message, height);

    if (error_code)
    {
        LOG_FATAL(LOG_NODE)
            << "Failure importing block for slot (" << reservation_->slot()
            << "), store is now corrupted: " << error_code.message();
        stop(error_code);
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

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in header index for slot (" << reservation_->slot()
            << ") " << ec.message();
        stop(ec);
        return false;
    }

    // When the queue is empty and a new header is announced the download is
    // not directed to the peer that made the announcement. This can lead to
    // delay in obtaining the block, which can be costly for mining. On the
    // other hand optimal mining relies on the compact block protocol, not full
    // block requests, so this is considered acceptable behavior here.

    send_get_blocks();
    return true;
}

// Fired by base timer and stop handler.
void protocol_block_sync::handle_event(const code& ec)
{
    if (stopped(ec))
    {
        // No longer receiving blocks, so free up the reservation.
        reservation_->stop();

        // Trigger unsubscribe or protocol will hang until next header indexed.
        chain_.unsubscribe();
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in block sync timer for slot (" << reservation_->slot()
            << ") " << ec.message();
        stop(ec);
        return;
    }

    // This ensures that a stall does not persist.
    if (reservation_->expired())
    {
        LOG_DEBUG(LOG_NODE)
            << "Restarting slow slot (" << reservation_->slot() << ") : ["
            << reservation_->size() << "]";
        stop(ec);
        return;
    }
}

} // namespace node
} // namespace libbitcoin
