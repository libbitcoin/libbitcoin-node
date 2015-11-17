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
#include <bitcoin/bitcoin.hpp>

INITIALIZE_TRACK(bc::node::protocol_block_sync);

namespace libbitcoin {
namespace node {

#define NAME "protocol_block_sync"
#define CLASS protocol_block_sync

using namespace bc::config;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

static const asio::duration one_minute(0, 1, 0);

protocol_block_sync::protocol_block_sync(threadpool& pool, p2p&,
    channel::ptr channel, uint32_t minimum_rate, size_t first_height,
    const hash_list& hashes)
  : protocol_timer(pool, channel, NAME),
    hash_index_(0),
    current_minute_(0),
    first_height_(first_height),
    minimum_rate_(minimum_rate),
    hashes_(hashes),
    CONSTRUCT_TRACK(protocol_block_sync, LOG_PROTOCOL)
{
}

size_t protocol_block_sync::current_height()
{
    return first_height_ + hash_index_;
}

size_t protocol_block_sync::target_height()
{
    return first_height_ + hashes_.size() - 1;
}

size_t protocol_block_sync::current_rate()
{
    return hash_index_ / current_minute_;
}

const hash_digest& protocol_block_sync::current_hash()
{
    return hashes_[hash_index_];
}

void protocol_block_sync::start(event_handler handler)
{
    if (peer_version().start_height < target_height())
    {
        log::info(LOG_NETWORK)
            << "Start height (" << peer_version().start_height
            << ") below block sync target (" << target_height() << ") from ["
            << authority() << "]";

        handler(error::channel_stopped);
        return;
    }

    auto complete = synchronize(BIND2(blocks_complete, _1, handler), 1, NAME);
    protocol_timer::start(one_minute, BIND2(handle_event, _1, complete));
    send_get_block(complete);
}

void protocol_block_sync::send_get_block(event_handler complete)
{
    if (stopped())
        return;

    if (current_height() == target_height())
    {
        complete(error::success);
        return;
    }
    
    const get_data packet{ { inventory_type_id::block, current_hash() } };

    SUBSCRIBE3(block, handle_receive, _1, _2, complete);
    SEND2(packet, handle_send, _1, complete);
}

void protocol_block_sync::handle_send(const code& ec, event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure sending get data to sync [" << authority() << "] "
            << ec.message();
        complete(ec);
    }
}

void protocol_block_sync::handle_receive(const code& ec, const block& message,
    event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure receiving block from sync ["
            << authority() << "] " << ec.message();
        complete(ec);
        return;
    }

    log::info(LOG_PROTOCOL)
        << "Synced block #" << current_height() << " from [" 
        << authority() << "]";

    ++hash_index_;
    send_get_block(complete);
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
        log::warning(LOG_PROTOCOL)
            << "Failure in block sync timer for [" << authority() << "] "
            << ec.message();
        complete(ec);
        return;
    }

    // It was a timeout, so one more minute has passed.
    ++current_minute_;

    // Drop the channel if it falls below the min sync rate.
    if (current_rate() < minimum_rate_)
    {
        log::info(LOG_PROTOCOL)
            << "Block sync rate (" << current_rate() << "/min) from ["
            << authority() << "]";
        complete(error::channel_timeout);
        return;
    }

    reset_timer();
}

void protocol_block_sync::blocks_complete(const code& ec,
    event_handler handler)
{
    // This is the original handler, feedback to the session.
    handler(ec);

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
