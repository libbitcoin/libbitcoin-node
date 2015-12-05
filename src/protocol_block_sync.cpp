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

#define NAME "block_sync"
#define CLASS protocol_block_sync

using namespace bc::config;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

// TODO: move to config.
static constexpr size_t block_rate_seconds = 10;

static constexpr size_t full_blocks = 50000;
static const asio::duration ten_seconds(0, 0, block_rate_seconds);

protocol_block_sync::protocol_block_sync(threadpool& pool, p2p&,
    channel::ptr channel, uint32_t minimum_rate, size_t first_height,
    size_t scope, const hash_list& hashes)
  : protocol_timer(pool, channel, true, NAME),
    current_second_(0),
    hash_index_(scope * full_blocks),
    start_index_(hash_index_),
    first_height_(first_height),
    target_height_(target(first_height, hash_index_, hashes)),
    minimum_rate_(minimum_rate),
    hashes_(hashes),
    CONSTRUCT_TRACK(protocol_block_sync)
{
    // TODO: convert to exceptions (public API).
    BITCOIN_ASSERT_MSG(scope < bc::max_size_t / full_blocks, "Invalid scope.");
    BITCOIN_ASSERT_MSG(hash_index_ < hashes.size(), "Invalid scope.");
    BITCOIN_ASSERT_MSG(!hashes_.empty(), "The block hash list is empty.");
}

// Utilities
// ----------------------------------------------------------------------------

size_t protocol_block_sync::target(size_t first_height, size_t start_index,
    const hash_list& hashes)
{
    const auto count = std::min(hashes.size() - start_index, full_blocks);
    return first_height + start_index + count - 1;
}

size_t protocol_block_sync::current_height() const
{
    return first_height_ + hash_index_;
}

size_t protocol_block_sync::current_rate() const
{
    return (hash_index_ - start_index_) / current_second_;
}

const hash_digest& protocol_block_sync::current_hash() const
{
    return hashes_[hash_index_];
}

message::get_data protocol_block_sync::build_get_data() const
{
    get_data packet;
    const auto copy = [&packet](const hash_digest& hash)
    {
        packet.inventories.push_back({ inventory_type_id::block, hash });
    };

    const auto start = hashes_.begin() + start_index_;
    const size_t count = target_height_ - first_height_ + 1;
    std::for_each(start, start + count, copy);
    return packet;
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start(event_handler handler)
{
    if (peer_version().start_height < target_height_)
    {
        log::info(LOG_NETWORK)
            << "Start height (" << peer_version().start_height
            << ") below block sync target (" << target_height_ << ") from ["
            << authority() << "]";

        handler(error::channel_stopped);
        return;
    }

    auto complete = synchronize(BIND2(blocks_complete, _1, handler), 1, NAME);
    protocol_timer::start(ten_seconds, BIND2(handle_event, _1, complete));

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
    if (current_hash() != message.header.hash())
    {
        log::info(LOG_PROTOCOL)
            << "Out of order block " << encode_hash(message.header.hash())
            << " from [" << authority() << "] (ignored)";

        // We either received a block anounce or we have a misbehaving peer.
        // Ignore and continue until success or hitting the rate limiter.
        return true;
    }

    log::info(LOG_PROTOCOL)
        << "Synced block #" << current_height() << " from ["
        << authority() << "]";

    ///////////////////////////////////////////////////
    // TODO: commit block here, from another thread. //
    ///////////////////////////////////////////////////

    ++hash_index_;

    // If we reached the target height the sync is complete.
    if (current_height() > target_height_)
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
    event_handler handler)
{
    // This is the end of the block sync sequence.
    handler(ec);

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
