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
#include <bitcoin/node/protocol_block_sync.hpp>
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

//// (401000 - 350000) / 6000 + 1 = 9 peers and 5666 per peer.
static constexpr size_t full_blocks = 50000;

// There is overflow risk only if full_blocks is 1 (with max_size_t hashes).
static_assert(full_blocks > 1, "unmitigated overflow risk");

session_block_sync::session_block_sync(p2p& network, const hash_list& hashes,
    size_t first_height, const settings& settings, block_chain& chain)
  : session_batch(network, false),
    offset_((hashes.size() - 0) / full_blocks + 1),
    first_height_(first_height),
    hashes_(hashes),
    settings_(settings),
    blockchain_(chain),
    CONSTRUCT_TRACK(session_block_sync)
{
}

// Start sequence.
// ----------------------------------------------------------------------------

void session_block_sync::start(result_handler handler)
{
    session::start(CONCURRENT2(handle_started, _1, handler));
}

void session_block_sync::handle_started(const code& ec, result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    if (hashes_.empty())
    {
        handler(error::success);
        return;
    }

    // Parallelize into full_blocks (50k) sized groups and synchronize.
    const auto complete = synchronize(handler, offset_, NAME);
    const auto connector = create_connector();

    // This is the end of the start sequence.
    for (size_t part = 0; part < offset_; ++part)
        new_connection(first_height_ + 0 + part, part, connector, complete);
}

// Block sync sequence.
// ----------------------------------------------------------------------------

void session_block_sync::new_connection(size_t start_height, size_t partition,
    connector::ptr connect, result_handler handler)
{
    if (stopped())
    {
        log::debug(LOG_SESSION)
            << "Suspending block sync partition (" << partition << ").";
        return;
    }

    log::debug(LOG_SESSION)
        << "Starting block sync partition (" << partition << ")";

    // BLOCK SYNC CONNECT
    this->connect(connect,
        BIND6(handle_connect, _1, _2, start_height, partition, connect, handler));
}

void session_block_sync::handle_connect(const code& ec, channel::ptr channel,
    size_t start_height, size_t partition, connector::ptr connect,
    result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_SESSION)
            << "Failure connecting block sync channel (" << partition
            << ") " << ec.message();
        new_connection(start_height, partition, connect, handler);
        return;
    }

    log::info(LOG_SESSION)
        << "Connected to block sync channel (" << partition << ") ["
        << channel->authority() << "]";

    register_channel(channel,
        BIND6(handle_channel_start, _1, start_height, partition, connect, channel, handler),
        BIND2(handle_channel_stop, _1, partition));
}

void session_block_sync::handle_channel_start(const code& ec,
    size_t start_height, size_t partition, connector::ptr connect,
    channel::ptr channel, result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, start_height, partition, connect, handler);
        return;
    }

    const auto byte_rate = settings_.block_bytes_per_second;

    attach<protocol_ping>(channel)->start();
    attach<protocol_address>(channel)->start();
    attach<protocol_block_sync>(channel, first_height_, start_height, offset_,
        byte_rate, hashes_, blockchain_)->start(
            BIND5(handle_complete, _1, _2, partition, connect, handler));
};

// We ignore the result code here.
void session_block_sync::handle_complete(const code&, size_t start_height,
    size_t partition, network::connector::ptr connect, result_handler handler)
{
    BITCOIN_ASSERT(start_height >= first_height_);
    const auto index = start_height - first_height_;

    // There is no failure scenario, loop until done (add timer).
    if (index < hashes_.size())
    {
        new_connection(start_height, partition, connect, handler);
        return;
    }

    // This is the end of the block sync sequence.
    handler(error::success);
}

void session_block_sync::handle_channel_stop(const code& ec, size_t partition)
{
    log::debug(LOG_SESSION)
        << "Block sync channel (" << partition << ") stopped: "
        << ec.message();
}

} // namespace node
} // namespace libbitcoin
