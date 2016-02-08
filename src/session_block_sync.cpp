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
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocol_block_sync.hpp>

INITIALIZE_TRACK(bc::node::session_block_sync);

namespace libbitcoin {
namespace node {

#define CLASS session_block_sync
#define NAME "session_block_sync"

using namespace config;
using namespace network;
using std::placeholders::_1;
using std::placeholders::_2;

static constexpr size_t full_blocks = 50000;

// There is overflow risk only if full_blocks is 1 (with max_size_t hashes).
static_assert(full_blocks > 1, "unmitigated overflow risk");

session_block_sync::session_block_sync(threadpool& pool, p2p& network,
    const hash_list& hashes, size_t first_height,
    const configuration& configuration)
  : session_batch(pool, network, configuration.network, false),
    hashes_(hashes),
    first_height_(first_height),
    configuration_(configuration),
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

    // Parallelize into full_blocks (50k) sized groups and synchronize.
    const auto scopes = (hashes_.size() / full_blocks) + 1;
    const auto complete = synchronize(handler, scopes, NAME);
    const auto connector = create_connector();

    // This is the end of the start sequence.
    for (size_t scope = 0; scope < scopes; ++scope)
    {
        const auto start_index = scope * full_blocks;
        const auto start_height = first_height_ + start_index;

        const auto remainder = hashes_.size() - start_index;
        const auto count = std::min(full_blocks, remainder);
        const auto end_height = start_height + count - 1;

        new_connection(start_height, end_height, connector, complete);
    }
}

// Header sync sequence.
// ----------------------------------------------------------------------------

void session_block_sync::new_connection(size_t start, size_t end,
    connector::ptr connect, result_handler handler)
{
    if (stopped())
    {
        log::debug(LOG_NETWORK)
            << "Suspending block sync session (" << start << ").";
        return;
    }

    log::debug(LOG_NETWORK)
        << "Starting block sync session (" << start << ")";

    // BLOCK SYNC CONNECT
    this->connect(connect,
        BIND6(handle_connect, _1, _2, start, end, connect, handler));
}

void session_block_sync::handle_connect(const code& ec, channel::ptr channel,
    size_t start, size_t end, connector::ptr connect, result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure connecting block sync channel (" << start
            << ") " << ec.message();
        new_connection(start, end, connect, handler);
        return;
    }

    log::info(LOG_NETWORK)
        << "Connected to block sync channel (" << start << ") ["
        << channel->authority() << "]";

    register_channel(channel,
        BIND6(handle_channel_start, _1, start, end, connect, channel, handler),
        BIND3(handle_channel_stop, _1, start, end));
}

void session_block_sync::handle_channel_start(const code& ec, size_t start,
    size_t end, connector::ptr connect, channel::ptr channel,
    result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, start, end, connect, handler);
        return;
    }

    const auto rate = configuration_.node.blocks_per_second;

    attach<protocol_ping>(channel)->start(settings_);
    attach<protocol_address>(channel)->start(settings_);
    attach<protocol_block_sync>(channel, first_height_, start, end, rate,
        hashes_)->start(BIND5(handle_complete, _1, _2, end, connect, handler));
};

// We ignore the result code here.
void session_block_sync::handle_complete(const code&, size_t start,
    size_t end, network::connector::ptr connect, result_handler handler)
{
    // There is no failure scenario, loop until done (add timer).
    if (start <= end)
    {
        new_connection(start, end, connect, handler);
        return;
    }

    // This is the end of the block sync sequence.
    handler(error::success);
}

void session_block_sync::handle_channel_stop(const code& ec, size_t start,
    size_t)
{
    log::debug(LOG_NETWORK)
        << "Block sync channel (" << start << ") stopped: "
        << ec.message();
}

} // namespace node
} // namespace libbitcoin
