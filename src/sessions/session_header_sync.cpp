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
#include <bitcoin/node/sessions/session_header_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_sync.hpp>
#include <bitcoin/node/settings.hpp>
#include <bitcoin/node/utility/header_queue.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_header_sync

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::config;
using namespace bc::network;
using namespace std::placeholders;

// The minimum rate back off factor, must be < 1.
static constexpr float back_off_factor = 0.75f;

// The starting minimum header download rate, exponentially backs off.
static constexpr uint32_t headers_per_second = 10000;

// Sort is required here but not in configuration settings.
session_header_sync::session_header_sync(p2p& network, header_queue& hashes,
    block_chain_impl& blockchain)
  : session_batch(network, false),
    hashes_(hashes),
    minimum_rate_(headers_per_second),
    blockchain_(blockchain),
    CONSTRUCT_TRACK(session_header_sync)
{
    static_assert(back_off_factor < 1, "invalid back-off factor");
}

// Checkpoints are not sorted in config but must be here.
checkpoint::list session_header_sync::sort(checkpoint::list checkpoints)
{
    return checkpoint::sort(checkpoints);
}

// Start sequence.
// ----------------------------------------------------------------------------

void session_header_sync::start(result_handler handler)
{
    session::start(CONCURRENT2(handle_started, _1, handler));
}

void session_header_sync::handle_started(const code& ec,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    if (!hashes_.empty())
    {
        log::error(LOG_NODE)
            << "Header hash list must not be initialized.";
        handler(error::operation_failed);
        return;
    }

    checkpoint first;
    auto code = session_header_sync::get_first(first, blockchain_);

    if (code)
    {
        log::error(LOG_NODE)
            << "Error getting header sync first: " << code.message();
        handler(code);
        return;
    }

    code = session_header_sync::get_last(last_, blockchain_);

    if (code)
    {
        log::error(LOG_NODE)
            << "Error getting header sync last: " << code.message();
        handler(code);
        return;
    }

    if (first == last_)
    {
        handler(error::success);
        return;
    }

    hashes_.initialize(first);

    // This is the end of the start sequence.
    new_connection(create_connector(), handler);
}

// Header sync sequence.
// ----------------------------------------------------------------------------

void session_header_sync::new_connection(connector::ptr connect,
    result_handler handler)
{
    if (stopped())
    {
        log::debug(LOG_NODE)
            << "Suspending header sync session.";
        return;
    }

    // HEADER SYNC CONNECT
    this->connect(connect, BIND4(handle_connect, _1, _2, connect, handler));
}

void session_header_sync::handle_connect(const code& ec, channel::ptr channel,
    connector::ptr connect, result_handler handler)
{
    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure connecting header sync channel: " << ec.message();
        new_connection(connect, handler);
        return;
    }

    log::debug(LOG_NODE)
        << "Connected to header sync channel [" << channel->authority() << "]";

    register_channel(channel,
        BIND4(handle_channel_start, _1, connect, channel, handler),
        BIND1(handle_channel_stop, _1));
}

void session_header_sync::handle_channel_start(const code& ec,
    connector::ptr connect, channel::ptr channel, result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, connect, handler);
        return;
    }

    attach<protocol_ping>(channel)->start();
    attach<protocol_address>(channel)->start();
    attach<protocol_header_sync>(channel, hashes_, minimum_rate_, last_)
        ->start(BIND3(handle_complete, _1, connect, handler));
};

void session_header_sync::handle_complete(const code& ec,
    network::connector::ptr connect, result_handler handler)
{
    if (!ec)
    {
        // This is the end of the header sync sequence.
        handler(ec);
        return;
    }

    // Reduce the rate minimum so that we don't get hung up.
    minimum_rate_ = static_cast<uint32_t>(minimum_rate_ * back_off_factor);

    // There is no failure scenario, we ignore the result code here.
    new_connection(connect, handler);
}

void session_header_sync::handle_channel_stop(const code& ec)
{
    log::debug(LOG_NODE)
        << "Header sync channel stopped: " << ec.message();
}

// Utility.
// ----------------------------------------------------------------------------

// Using config and the blockchain detemine header sync first checkpoint.
code session_header_sync::get_first(checkpoint& out_first,
    block_chain_impl& blockchain)
{
    uint64_t gap_height;
    static constexpr size_t genesis_height = 0;

    if (!blockchain.get_next_gap(gap_height, genesis_height))
        return error::operation_failed;

    header first_header;
    const auto first_height = gap_height - 1;

    if (!blockchain.get_header(first_header, first_height))
        return error::not_found;

    out_first = std::move(checkpoint{ first_header.hash(), first_height });
    return error::success;
}

// Using config and the blockchain detemine header sync last checkpoint.
code session_header_sync::get_last(checkpoint& out_last,
    block_chain_impl& blockchain)
{
    size_t top_height;

    if (!blockchain.get_last_height(top_height))
        return error::operation_failed;

    header last_header;
    hash_digest last_hash;
    const auto& checkpoints = blockchain.chain_settings().checkpoints;

    // Select the higher of the top or the last of any checkpoints.
    const auto last_height = checkpoints.empty() ? top_height :
        std::max(checkpoints.back().height(), top_height);

    if (last_height != top_height)
        last_hash = checkpoints.back().hash();
    else if (blockchain.get_header(last_header, last_height))
        last_hash = last_header.hash();
    else
        return error::not_found;

    out_last = std::move(checkpoint{ last_hash, last_height });
    return error::success;
}

} // namespace node
} // namespace libbitcoin
