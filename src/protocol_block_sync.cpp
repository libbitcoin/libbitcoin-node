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
#include <bitcoin/bitcoin.hpp>

INITIALIZE_TRACK(bc::node::protocol_block_sync);

namespace libbitcoin {
namespace node {

#define NAME "block_sync"
#define CLASS protocol_block_sync

using namespace bc::config;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

// TODO: move to config.
static constexpr size_t block_rate_seconds = 10;

static const asio::seconds block_rate(block_rate_seconds);

// TODO: pass end-height vs. count.
protocol_block_sync::protocol_block_sync(threadpool& pool, p2p&,
    channel::ptr channel, size_t first_height, size_t start_height,
    size_t end_height, uint32_t minimum_rate, const hash_list& hashes)
  : protocol_timer(pool, channel, true, NAME),
    current_second_(0),
    index_(start_height - first_height),
    first_height_(first_height),
    start_height_(start_height),
    end_height_(end_height),
    count_(end_height - start_height + 1),
    minimum_rate_(minimum_rate),
    hashes_(hashes),
    CONSTRUCT_TRACK(protocol_block_sync)
{
    BITCOIN_ASSERT(start_height <= end_height);
    BITCOIN_ASSERT(first_height_ <= start_height_);
    BITCOIN_ASSERT(count_ <= hashes_.size());
    BITCOIN_ASSERT(index_ <= hashes_.size() - count_);
}

// Utilities.
// ----------------------------------------------------------------------------

size_t protocol_block_sync::current_rate() const
{
    return (next_height() - start_height_) / current_second_;
}

size_t protocol_block_sync::next_height() const
{
    return first_height_ + index_;
}

const hash_digest& protocol_block_sync::next_hash() const
{
    return hashes_[index_];
}

message::get_data protocol_block_sync::build_get_data() const
{
    get_data packet;
    const auto copy = [&packet](const hash_digest& hash)
    {
        packet.inventories.push_back({ inventory_type_id::block, hash });
    };

    const auto start = hashes_.begin() + index_;
    std::for_each(start, start + count_, copy);

    log::info(LOG_NETWORK)
        << "Count: " << count_
        << ", start_index: " << index_
        << ", first_height: " << first_height_
        << ", start_height: " << start_height_
        << ", end_height: " << end_height_;

    return packet;
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start(count_handler handler)
{
    // version.start_height is the top of the peer's chain.
    if (peer_version().start_height < end_height_)
    {
        log::info(LOG_NETWORK)
            << "Start height (" << peer_version().start_height
            << ") below block sync target (" << end_height_ << ") from ["
            << authority() << "]";

        handler(error::channel_stopped, next_height());
        return;
    }

    auto complete = synchronize(BIND2(blocks_complete, _1, handler), 1, NAME);
    protocol_timer::start(block_rate, BIND2(handle_event, _1, complete));

    SUBSCRIBE3(block, handle_receive, _1, _2, complete);

    // This is the end of the start sequence.
    send_get_blocks(complete);
}

// Block sync sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::send_get_blocks(event_handler complete)
{
    if (stopped())
        return;

    // This is sent only once in this protocol, for a maximum of 50k blocks.
    SEND2(build_get_data(), handle_send, _1, complete);
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

bool protocol_block_sync::handle_receive(const code& ec, const block& message,
    event_handler complete)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure receiving block from sync ["
            << authority() << "] " << ec.message();
        complete(ec);
        return false;
    }

    // A block must match the request in order to be accepted.
    if (next_hash() != message.header.hash())
    {
        log::info(LOG_PROTOCOL)
            << "Out of order block " << encode_hash(message.header.hash())
            << " from [" << authority() << "] (ignored)";

        // We either received a block anounce or we have a misbehaving peer.
        // Ignore and continue until success or hitting the rate limiter.
        return true;
    }

    log::info(LOG_PROTOCOL)
        << "Synced block #" << next_height() << " from ["
        << authority() << "]";

    //////////////////////////////
    // TODO: commit block here. //
    //////////////////////////////

    ++index_;

    // If our next block is past the end height the sync is complete.
    if (next_height() > end_height_)
    {
        complete(error::success);
        return false;
    }

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
        log::warning(LOG_PROTOCOL)
            << "Failure in block sync timer for [" << authority() << "] "
            << ec.message();
        complete(ec);
        return;
    }

    // It was a timeout, so ten more seconds have passed.
    current_second_ += block_rate_seconds;

    // Drop the channel if it falls below the min sync rate.
    if (current_rate() < minimum_rate_)
    {
        log::info(LOG_PROTOCOL)
            << "Block sync rate (" << current_rate() << "/min) from ["
            << authority() << "]";
        complete(error::channel_timeout);
        return;
    }
}

void protocol_block_sync::blocks_complete(const code& ec,
    count_handler handler)
{
    // This is the end of the block sync sequence.
    handler(ec, next_height());

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
