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
#include <bitcoin/node/protocols/protocol_block_in.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <string>
#include <boost/format.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block_in"
#define CLASS protocol_block_in

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::chrono;
using namespace std::placeholders;

static constexpr auto perpetual_timer = true;

static inline uint32_t get_poll_seconds(const node::settings& settings)
{
    // Set 136 years as equivalent to "never" if configured to disable.
    const auto value = settings.block_poll_seconds;
    return value == 0 ? max_uint32 : value;
}

protocol_block_in::protocol_block_in(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_timer(node, channel, perpetual_timer, NAME),
    node_(node),
    chain_(chain),
    last_locator_top_(null_hash),
    block_poll_seconds_(get_poll_seconds(node.node_settings())),

    // TODO: move send_headers to a derived class protocol_block_in_70012.
    headers_from_peer_(negotiated_version() >= version::level::bip130),

    // This patch is treated as integral to basic block handling.
    blocks_from_peer_(
        negotiated_version() > version::level::no_blocks_end ||
        negotiated_version() < version::level::no_blocks_start),
    CONSTRUCT_TRACK(protocol_block_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_in::start()
{
    // Use perpetual protocol timer to prevent stall (our heartbeat).
    protocol_timer::start(asio::seconds(block_poll_seconds_),
        BIND1(get_block_inventory, _1));

    // TODO: move headers to a derived class protocol_block_in_31800.
    SUBSCRIBE2(headers, handle_receive_headers, _1, _2);

    // TODO: move not_found to a derived class protocol_block_in_70001.
    SUBSCRIBE2(not_found, handle_receive_not_found, _1, _2);
    SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    // TODO: move send_headers to a derived class protocol_block_in_70012.
    if (headers_from_peer_)
    {
        // Ask peer to send headers vs. inventory block announcements.
        SEND2(send_headers(), handle_send, _1, send_headers::command);
    }

    // Subscribe to block acceptance notifications (for gap fill).
    chain_.subscribe_reorganize(BIND4(handle_reorganized, _1, _2, _3, _4));

    // Send initial get_[blocks|headers] message by simulating first heartbeat.
    // Since we need blocks do not stay connected to peer in bad version range.
    set_event(blocks_from_peer_ ? error::success : error::channel_stopped);
}

// Send get_[headers|blocks] sequence.
//-----------------------------------------------------------------------------

// This is fired by the callback (i.e. base timer and stop handler).
void protocol_block_in::get_block_inventory(const code& ec)
{
    if (stopped(ec))
    {
        // This may get called more than once per stop.
        handle_stop(ec);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        LOG_DEBUG(LOG_NODE)
            << "Failure in block timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    send_get_blocks(null_hash);
}

void protocol_block_in::send_get_blocks(const hash_digest& stop_hash)
{
    const auto chain_top = node_.top_block();
    const auto& chain_top_hash = chain_top.hash();
    const auto last_locator_top = last_locator_top_.load();

    // Avoid requesting from the same start as last request to this peer.
    // This does not guarantee prevention, it's just an optimization.
    // If the peer does not respond to the previous request this will stall
    // unless a block announcement is connected or another channel advances.
    if (chain_top_hash != null_hash && chain_top_hash == last_locator_top)
        return;

    const auto heights = block::locator_heights(chain_top.height());

    chain_.fetch_block_locator(heights,
        BIND3(handle_fetch_block_locator, _1, _2, stop_hash));
}

void protocol_block_in::handle_fetch_block_locator(const code& ec,
    get_headers_ptr message, const hash_digest& stop_hash)
{
    if (stopped(ec))
        return;

    const auto& last_hash = message->start_hashes().front();

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure generating block locator for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->start_hashes().empty())
        return;

    // TODO: move get_headers to a derived class protocol_block_in_31800.
    const auto use_headers = negotiated_version() >= version::level::headers;
    const auto default_size = use_headers ? "2000" : "500";

    LOG_DEBUG(LOG_NODE)
        << "Ask [" << authority() << "] for headers from ["
        << encode_hash(last_hash) << "] through [" <<
        (stop_hash == null_hash ? default_size : encode_hash(stop_hash)) << "]";

    // Save the locator top to prevent a redundant future request.
    last_locator_top_.store(last_hash);
    message->set_stop_hash(stop_hash);

    if (use_headers)
        SEND2(*message, handle_send, _1, message->command);
    else
        SEND2(static_cast<get_blocks>(*message), handle_send, _1,
            message->command);
}

// Receive headers|inventory sequence.
//-----------------------------------------------------------------------------

// TODO: move headers to a derived class protocol_block_in_31800.
// This originates from send_header->annoucements and get_headers requests.
bool protocol_block_in::handle_receive_headers(const code& ec,
    headers_const_ptr message)
{
    if (stopped(ec))
        return false;

    // There is no benefit to this use of headers, in fact it is suboptimal.
    // In v3 headers will be used to build block tree before getting blocks.
    const auto response = std::make_shared<get_data>();
    message->to_inventory(response->inventories(), inventory::type_id::block);

    // Remove hashes of blocks that we already have.
    // BUGBUG: this removes blocks that are not in the main chain.
    chain_.filter_blocks(response, BIND2(send_get_data, _1, response));
    return true;
}

// This originates from default annoucements and get_blocks requests.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    inventory_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto response = std::make_shared<get_data>();
    message->reduce(response->inventories(), inventory::type_id::block);

    // Remove hashes of blocks that we already have.
    // BUGBUG: this removes blocks that are not in the main chain.
    chain_.filter_blocks(response, BIND2(send_get_data, _1, response));
    return true;
}

void protocol_block_in::send_get_data(const code& ec, get_data_ptr message)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure filtering block hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->inventories().empty())
        return;

    // inventory|headers->get_data[blocks]
    SEND2(*message, handle_send, _1, message->command);
}

// Receive not_found sequence.
//-----------------------------------------------------------------------------

// TODO: move not_found to a derived class protocol_block_in_70001.
bool protocol_block_in::handle_receive_not_found(const code& ec,
    not_found_const_ptr message)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        LOG_DEBUG(LOG_NODE)
            << "Failure getting block not_found from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list hashes;
    message->to_hashes(hashes, inventory::type_id::block);

    // The peer cannot locate a block that it told us it had.
    // This only results from reorganization assuming peer is proper.
    for (const auto hash: hashes)
    {
        LOG_DEBUG(LOG_NODE)
            << "Block not_found [" << encode_hash(hash) << "] from ["
            << authority() << "]";
    }

    return true;
}

// Receive block sequence.
//-----------------------------------------------------------------------------

bool protocol_block_in::handle_receive_block(const code& ec,
    block_const_ptr message)
{
    if (stopped(ec))
        return false;

    // Reset the timer because we just received a block from this peer.
    // Once we are at the top this will end up polling the peer.
    reset_timer();

    message->validation.originator = nonce();
    chain_.organize(message, BIND2(handle_store_block, _1, message));
    return true;
}

// The transaction has been saved to the block chain (or not).
// This will be picked up by subscription in block_out and will cause the block
// to be announced to non-originating peers.
void protocol_block_in::handle_store_block(const code& ec,
    block_const_ptr message)
{
    if (stopped(ec))
        return;

    const auto hash = message->header().hash();

    // Ask the peer for blocks from the chain top up to this orphan.
    if (ec == error::orphan_block)
        send_get_blocks(hash);

    const auto encoded = encode_hash(hash);

    if (ec == error::orphan_block ||
        ec == error::duplicate_block ||
        ec == error::insufficient_work)
    {
        LOG_DEBUG(LOG_NODE)
            << "Captured block [" << encoded << "] from [" << authority()
            << "] " << ec.message();
        return;
    }

    if (ec)
    {
        LOG_DEBUG(LOG_NODE)
            << "Rejected block [" << encoded << "] from [" << authority()
            << "] " << ec.message();
        stop(ec);
        return;
    }

    const auto state = message->validation.state;
    BITCOIN_ASSERT(state);

    // Show that diplayed forks may be missing activations due to checkpoints.
    const auto checked = state->is_under_checkpoint() ? "*" : "";

    LOG_DEBUG(LOG_NODE)
        << "Connected block [" << encoded << "] at height [" << state->height()
        << "] from [" << authority() << "] (" << state->enabled_forks()
        << checked << ", " << state->minimum_version() << ").";

    report(*message);
}

// Subscription.
//-----------------------------------------------------------------------------

// At least one block was accepted into the chain, originating from any peer.
bool protocol_block_in::handle_reorganized(code ec, size_t fork_height,
    block_const_ptr_list_const_ptr incoming, block_const_ptr_list_const_ptr)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure handling reorganization for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    ////// Report the blocks that originated from this peer.
    ////// If originating peer is dropped there will be no report here.
    ////for (const auto block: *incoming)
    ////    if (block->validation.originator == nonce())
    ////        LOG_DEBUG(LOG_NODE)
    ////            << "Reorganized block [" << encode_hash(block->header().hash())
    ////            << "] from [" << authority() << "].";

    return true;
}

void protocol_block_in::handle_stop(const code&)
{
    LOG_DEBUG(LOG_NETWORK)
        << "Stopped block_in protocol for [" << authority() << "].";
}

// Block reporting.
//-----------------------------------------------------------------------------

inline bool enabled(size_t height)
{
    // Vary the reporting performance reporting interval by height.
    const auto modulus =
        (height < 100000 ? 100 :
        (height < 200000 ? 10 : 1));

    return height % modulus == 0;
}

inline float difference(const asio::time_point& start,
    const asio::time_point& end)
{
    const auto elapsed = duration_cast<asio::microseconds>(end - start);
    return static_cast<float>(elapsed.count());
}

inline size_t unit_cost(const asio::time_point& start,
    const asio::time_point& end, size_t value)
{
    return static_cast<size_t>(std::round(difference(start, end) / value));
}

inline size_t total_cost_ms(const asio::time_point& start,
    const asio::time_point& end)
{
    static constexpr size_t microseconds_per_millisecond = 1000;
    return unit_cost(start, end, microseconds_per_millisecond);
}

void protocol_block_in::report(const chain::block& block)
{
    BITCOIN_ASSERT(block.validation.state);
    const auto height = block.validation.state->height();

    if (enabled(height))
    {
        const auto& times = block.validation;
        const auto now = asio::steady_clock::now();
        const auto transactions = block.transactions().size();
        const auto inputs = std::max(block.total_inputs(), size_t(1));

        // Subtract total deserialization time from start of validation because
        // the wait time is between end_deserialize and start_check. This lets
        // us simulate block announcement validation time as there is no wait.
        const auto start_validate = times.start_check - 
            (times.end_deserialize - times.start_deserialize);

        boost::format format("Block [%|i|] %|4i| txs %|4i| ins "
            "%|4i| wms %|4i| vms %|4i| vµs %|4i| rµs %|4i| cµs %|4i| pµs "
            "%|4i| aµs %|4i| sµs %|4i| dµs %|f|");

        LOG_INFO(LOG_BLOCKCHAIN)
            << (format % height % transactions % inputs %

            // wait total (ms)
            total_cost_ms(times.end_deserialize, times.start_check) %

            // validation total (ms)
            total_cost_ms(start_validate, times.start_notify) %

            // validation per input (µs)
            unit_cost(start_validate, times.start_notify, inputs) %

            // deserialization (read) per input (µs)
            unit_cost(times.start_deserialize, times.end_deserialize, inputs) %

            // check per input (µs)
            unit_cost(times.start_check, times.start_populate, inputs) %

            // population per input (µs)
            unit_cost(times.start_populate, times.start_accept, inputs) %

            // accept per input (µs)
            unit_cost(times.start_accept, times.start_connect, inputs) %

            // connect (script) per input (µs)
            unit_cost(times.start_connect, times.start_notify, inputs) %

            // deposit per input (µs)
            unit_cost(times.start_push, times.end_push, inputs) %

            // this block transaction cache efficiency (hits/queries)
            block.validation.cache_efficiency);
    }
}

} // namespace node
} // namespace libbitcoin
