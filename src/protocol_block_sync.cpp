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

INITIALIZE_TRACK(bc::node::protocol_block_sync);

namespace libbitcoin {
namespace node {

#define NAME "block_sync"
#define CLASS protocol_block_sync

using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

// We measure the block rate in a moving 10 second window.
static constexpr size_t block_rate_window_seconds = 10;
static const asio::seconds block_rate(block_rate_window_seconds);

protocol_block_sync::protocol_block_sync(p2p& network,
    channel::ptr channel, size_t first_height, size_t start_height,
    size_t offset, uint32_t minimum_rate, const hash_list& hashes,
    block_chain& chain)
  : protocol_timer(network, channel, true, NAME),
    byte_count_(0),
    index_(start_height - first_height),
    first_height_(first_height),
    start_height_(start_height),
    offset_(offset),
    channel_(start_height % offset),
    minimum_rate_(minimum_rate),
    hashes_(hashes),
    blockchain_(chain),
    CONSTRUCT_TRACK(protocol_block_sync)
{
    BITCOIN_ASSERT(index_ < hashes_.size());
    BITCOIN_ASSERT(first_height_ <= start_height_);
}

// Utilities.
// ----------------------------------------------------------------------------

size_t protocol_block_sync::current_rate() const
{
    return byte_count_ / block_rate_window_seconds;
}

size_t protocol_block_sync::current_height() const
{
    return first_height_ + index_;
}

const hash_digest& protocol_block_sync::current_hash() const
{
    return hashes_[index_];
}

bool protocol_block_sync::next_block(const block& message)
{
    const auto block_size = message.serialized_size();

    BITCOIN_ASSERT(byte_count_ <= max_size_t - block_size);
    byte_count_ += block_size;

    BITCOIN_ASSERT(index_ <= max_size_t - offset_);
    index_ += offset_;

    return index_ < hashes_.size();
}

// TODO: build a chunk at a time until complete.
// TODO: start with one chunk and ask for next chunk as soon as backlog goes
// below chunk size. This ensures that backlog will remain between 1-2 chunks
// until there are no more to request. If backlog drops to zero then close
// channel. Don't close a slow channel while it still has backlog.
message::get_data protocol_block_sync::build_get_data() const
{
    get_data packet;
    for (auto index = index_.load(); index < hashes_.size(); index += offset_)
    {
        const auto& hash = hashes_[index];
        packet.inventories.push_back({ inventory_type_id::block, hash });
    }

    return packet;
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_block_sync::start(count_handler handler)
{
    const auto peer_top = peer_version().start_height;
    const auto headers_top = first_height_ + hashes_.size() - 1;

    if (peer_top < headers_top)
    {
        log::info(LOG_PROTOCOL)
            << "Start height (" << peer_top << ") below block sync target ("
            << headers_top << ") from [" << authority() << "]";

        handler(error::channel_stopped, current_height());
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
            << "Failure receiving block from sync ["
            << authority() << "] " << ec.message();
        complete(ec);
        return false;
    }

    // A block must match the request in order to be accepted.
    if (current_hash() != message->header.hash())
    {
        log::warning(LOG_PROTOCOL)
            << "Out of order block " << encode_hash(message->header.hash())
            << " from [" << authority() << "] (ignored)";

        // We either received a block anounce or we have a misbehaving peer.
        // Ignore and continue until success or hitting the rate limiter.
        return true;
    }

    const auto height = current_height();

    // Block commit block here.
    if (blockchain_.import(message, height))
    {
        log::info(LOG_PROTOCOL)
            << "Imported block #" << height << " for (" << channel_
            << ") from [" << authority() << "]";
    }

    // If our next block is below the end the sync is incomplete.
    if (next_block(*message))
        return true;

    // This is the end of the sync loop.
    complete(error::success);
    return false;
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

    // BUGBUG: this causes unnecessary timeout of channels simply because of
    // lock contention. Instead drop a channel if it drops to zero entries.

    // Drop the channel if it falls below the min sync rate in the window.
    if (current_rate() < minimum_rate_)
    {
        log::info(LOG_PROTOCOL)
            << "Block sync rate (" << current_rate() << "/sec) from ["
            << authority() << "]";
        complete(error::channel_timeout);
        return;
    }

    // Reset bytes-per-period accumulator.
    byte_count_ = 0;
}

void protocol_block_sync::blocks_complete(const code& ec,
    count_handler handler)
{
    // This is the end of the block sync sequence.
    handler(ec, current_height());

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
