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
#include <bitcoin/node/protocols/protocol_block_in.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
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

inline bool is_witness(uint64_t services)
{
    return (services & version::service::node_witness) != 0;
}

protocol_block_in::protocol_block_in(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_timer(node, channel, false, NAME),
    node_(node),
    chain_(chain),
    block_latency_(node.node_settings().block_latency()),

    // TODO: move no-sync to a derived class protocol_block_in_70001.
    not_found_(negotiated_version() < version::level::bip37),

    // TODO: move no-sync to a derived class protocol_block_in_31800.
    blocks_first_(negotiated_version() < version::level::headers),

    // TODO: move no-inventory to a derived class protocol_block_in_70012.
    blocks_inventory_(negotiated_version() < version::level::bip130),

    // TODO: apply this only in protocol_block_in_70001, where it is relevant.
    blocks_from_peer_(
        negotiated_version() > version::level::no_blocks_end ||
        negotiated_version() < version::level::no_blocks_start),

    // Witness must be requested if possibly enforced.
    require_witness_(is_witness(node.network_settings().services)),
    peer_witness_(is_witness(channel->peer_version()->services())),
    CONSTRUCT_TRACK(protocol_block_in)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_in::start()
{
    // Use timer to drop slow peers.
    protocol_timer::start(block_latency_, BIND1(handle_timeout, _1));

    // Can't stop in start, so the timer will close the channel.
    if (!blocks_from_peer_)

    // Do not process incoming blocks if required witness is unavailable.
    // The channel will remain active outbound unless node becomes stale.
    if (require_witness_ && !peer_witness_)
        return;

    // TODO: move not_found to a derived class protocol_block_in_70001.
    if (not_found_)
    {
        SUBSCRIBE2(not_found, handle_receive_not_found, _1, _2);
    }

    // TODO: move no-inventory to a derived class protocol_block_in_70012.
    if (blocks_inventory_)
    {
        SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    }

    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    // TODO: move no-sync to a derived class protocol_block_in_31800.
    if (blocks_first_)
        send_get_blocks(null_hash);
}

// Send get_blocks sequence.
//-----------------------------------------------------------------------------

void protocol_block_in::send_get_blocks(const hash_digest& stop_hash)
{
    const auto heights = block::locator_heights(node_.top_block().height());

    // Even though we are asking for blocks, we are going to use the headers.
    chain_.fetch_header_locator(heights,
        BIND3(handle_fetch_header_locator, _1, _2, stop_hash));
}

void protocol_block_in::handle_fetch_header_locator(const code& ec,
    get_blocks_ptr message, const hash_digest& stop_hash)
{
    if (stopped(ec))
        return;

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

    message->set_stop_hash(stop_hash);
    const auto& last_hash = message->start_hashes().front();

    if (stop_hash == null_hash)
    {
        LOG_DEBUG(LOG_NODE)
            << "Ask [" << authority() << "] for block inventory after ["
            << encode_hash(last_hash) << "]";
    }
    else
    {
        LOG_DEBUG(LOG_NODE)
            << "Ask [" << authority() << "] for block inventory from ["
            << encode_hash(last_hash) << "] through ["
            << encode_hash(stop_hash) << "]";
    }

    SEND2(*message, handle_send, _1, message->command);
}

// Receive inventory sequence.
//-----------------------------------------------------------------------------

// This originates from default annoucements and get_blocks requests, or from
// an unsolicited announcement. There is no way to distinguish.
bool protocol_block_in::handle_receive_inventory(const code& ec,
    inventory_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto response = std::make_shared<get_data>();
    message->reduce(response->inventories(), inventory::type_id::block);

    if (response->inventories().size() > max_get_blocks)
    {
        LOG_WARNING(LOG_NODE)
            << "Block inventory from [" << authority() << "] exceeds "
            << max_get_blocks << " entires.";
        stop(ec);
        return false;
    }

    // Remove hashes of blocks that we already have.
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

    // Convert requested message types to corresponding witness types.
    if (require_witness_)
        message->to_witness();

    // If true if there was no existing backlog, so the timer must be started.
    if (backlog_.enqueue(message))
        reset_timer();

    // inventory->get_data[blocks]
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
        LOG_ERROR(LOG_NODE)
            << "Failure getting block not_found from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list hashes;
    message->to_hashes(hashes, inventory::type_id::block);

    for (const auto& hash: hashes)
    {
        LOG_DEBUG(LOG_NODE)
            << "Block not_found [" << encode_hash(hash) << "] from ["
            << authority() << "]";
    }

    // The peer cannot locate one or more blocks that it told us it had.
    // This only results from reorganization assuming peer is proper.
    // Drop the peer so next channgel generates a new locator and backlog.
    if (!hashes.empty())
        stop(error::channel_stopped);

    return true;
}

// Receive block sequence.
//-----------------------------------------------------------------------------

bool protocol_block_in::handle_receive_block(const code& ec,
    block_const_ptr message)
{
    if (stopped(ec))
        return false;

    // If a peer sends a block unannounced we drop the peer - always. However
    // it is common for block announcements to cause block requests to be sent
    // out of backlog order due to interleaving of threads. This results in
    // channel drops during initial block download but not after sync.
    if (!backlog_.dequeue(message->hash()))
    {
        LOG_DEBUG(LOG_NODE)
            << "Block [" << encode_hash(message->hash())
            << "] unexpected or out of order from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    if (!require_witness_ && message->is_segregated())
    {
        LOG_DEBUG(LOG_NODE)
            << "Block [" << encode_hash(message->hash())
            << "] contains unrequested witness from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    // TODO: REPORTING HEIGHT UNKNOWN HERE, GET FROM ORGANIZE.
    const size_t height = 42;

    message->header().metadata.originator = nonce();

    // TODO: patch compile.
    ////chain_.organize(message, BIND3(handle_store_block, _1, height, message));

    // Sending a new request will reset the timer upon inventory->get_data, but
    // we need to time out the lack of response to those requests when stale.
    // So we rest the timer in case of cleared and for not cleared.
    reset_timer();

    // TODO: move no-sync to a derived class protocol_block_in_31800.
    // Empty after pop means we need to make a new request.
    if (backlog_.empty() && blocks_first_)
        send_get_blocks(null_hash);

    return true;
}

// The block has been saved to the block chain (or not).
// This will be picked up by subscription in block_out and will cause the block
// to be announced to non-originating peers.
void protocol_block_in::handle_store_block(const code& ec, size_t height,
    block_const_ptr message)
{
    if (stopped(ec))
        return;

    const auto hash = message->hash();

    // Ask the peer for blocks from the chain top up to this orphan.
    // TODO: move no-inventory to a derived class protocol_block_in_70012.
    if (ec == error::orphan_block && blocks_inventory_)
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

    // TODO: send reject as applicable.
    if (ec)
    {
        LOG_DEBUG(LOG_NODE)
            << "Rejected block [" << encoded << "] from [" << authority()
            << "] " << ec.message();
        stop(ec);
        return;
    }

    // State may not be populated by metadata.
    const auto state = message->header().metadata.state;

    if (state)
    {
        // Show diplayed forks may be missing activations due to checkpoints.
        const auto checked = state->is_under_checkpoint() ? "*" : "";

        LOG_DEBUG(LOG_NODE)
            << "Connected block [" << encoded << "] at height ["
            << state->height() << "] from [" << authority() << "] ("
            << state->enabled_forks() << checked << ", "
            << state->minimum_block_version() << ").";
    }
    else
    {
        LOG_DEBUG(LOG_NODE)
            << "Connected block [" << encoded << "] at height [" << height
            << "] with no state.";
    }

    report(*message, height);
}

// Subscription.
//-----------------------------------------------------------------------------

// This is fired by the callback (i.e. base timer and stop handler).
void protocol_block_in::handle_timeout(const code& ec)
{
    if (stopped(ec))
    {
        // This may get called more than once per stop.
        handle_stop(ec);
        return;
    }

    // Since we need blocks do not stay connected to peer in bad version range.
    if (!blocks_from_peer_)
    {
        stop(error::channel_stopped);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure in block timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    // Can only end up here if time was not extended.
    if (!backlog_.empty())
    {
        LOG_DEBUG(LOG_NODE)
            << "Peer [" << authority()
            << "] exceeded configured block latency.";
        stop(error::channel_stopped);
        return;
    }

    // Can only end up here if peer did not respond to inventory or get_data.
    // At this point we are caught up with an honest peer. But if we are stale
    // we should try another peer and not just keep pounding this one.
    if (chain_.is_blocks_stale())
    {
        LOG_DEBUG(LOG_NODE)
            << "Peer [" << authority() << "] is stale.";
        stop(error::channel_stopped);
        return;
    }

    // If we are not stale then we are either good or stalled until peer sends
    // an announcement. There is no sense pinging a broken peer, so we either
    // drop the peer after a certain mount of time (above 10 minutes) or rely
    // on other peers to keep us moving and periodically age out connections.
    // Note that this allows a non-witness peer to hang on indefinately to our
    // witness-requiring node until the node becomes stale. Allowing this then
    // depends on requiring witness peers for explicitly outbound connections.
}

void protocol_block_in::handle_stop(const code&)
{
    LOG_VERBOSE(LOG_NETWORK)
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

void protocol_block_in::report(const chain::block& block, size_t height)
{
    if (enabled(height))
    {
        const auto& times = block.metadata;
        // const auto now = asio::steady_clock::now();
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
            block.metadata.cache_efficiency);
    }
}

} // namespace node
} // namespace libbitcoin
