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
#include <bitcoin/node/protocols/protocol_block.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block"
#define CLASS protocol_block

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

static constexpr auto locator_limit = 500u;
static constexpr auto perpetual_timer = true;
static constexpr auto send_headers_minium_version = 70012u;
static const auto get_blocks_interval = asio::seconds(1);

protocol_block::protocol_block(p2p& network, channel::ptr channel,
    block_chain& blockchain)
  : protocol_timer(network, channel, perpetual_timer, NAME),
    send_headers_(false),
    blockchain_(blockchain),
    CONSTRUCT_TRACK(protocol_block)
{
}

// Start sequence.
//-----------------------------------------------------------------------------

void protocol_block::start()
{
    // Use perpetual protocol timer to prevent stall.
    protocol_timer::start(get_blocks_interval, BIND1(send_get_blocks, _1));

    // Subscribe to block acceptance notifications.
    blockchain_.subscribe_reorganize(
        BIND4(handle_reorganized, _1, _2, _3, _4));

    SUBSCRIBE2(send_headers, handle_receive_send_headers, _1, _2);
    SUBSCRIBE2(get_headers, handle_receive_get_headers, _1, _2);
    SUBSCRIBE2(headers, handle_receive_headers, _1, _2);

    SUBSCRIBE2(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE2(inventory, handle_receive_inventory, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);
    SUBSCRIBE2(block, handle_receive_block, _1, _2);

    // This only affects peer's block announcements, timing not critical.
    send_send_headers();

    // Send initial get_blocks message by simulating first heartbeat.
    set_event(error::success);
}

// Receive get_blocks sequence.
//-----------------------------------------------------------------------------

bool protocol_block::handle_receive_get_blocks(const code& ec,
    message::get_blocks::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting get_blocks from [" << ec.message();
        stop(ec);
        return false;
    }

    // TODO: The threshold is the last block sent to this peer.
    const auto threshold = null_hash;

    blockchain_.fetch_locator_block_hashes(*message, threshold, locator_limit,
        BIND2(handle_fetch_locator_hashes, _1, _2));
    return true;
}

void protocol_block::handle_fetch_locator_hashes(const code& ec,
    const hash_list& hashes)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating locator block hashes for ["
            << authority() << "] " << ec.message();
        return;
    }

    // get_blocks->inventory
    const inventory response(hashes, inventory_type_id::block);
    SEND2(response, handle_send, _1, response.command);
}

// Receive get_headers sequence.
//-----------------------------------------------------------------------------

bool protocol_block::handle_receive_get_headers(const code& ec,
    message::get_headers::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting get_headers from [" << ec.message();
        stop(ec);
        return false;
    }

    // TODO: The threshold is the last block sent to this peer.
    const auto threshold = null_hash;

    blockchain_.fetch_locator_block_headers(*message, threshold, locator_limit,
        BIND2(handle_fetch_locator_headers, _1, _2));
    return true;
}

void protocol_block::handle_fetch_locator_headers(const code& ec,
    const chain::header::list& headers)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating locator block hashes for ["
            << authority() << "] " << ec.message();
        return;
    }

    // get_headers->headers
    const message::headers response(headers);
    SEND2(response, handle_send, _1, response.command);
}

// Send send_headers sequence.
//-----------------------------------------------------------------------------

// TODO: add version-to-named feature map on version class.
void protocol_block::send_send_headers()
{
    // send_headers
    if (peer_version().value >= send_headers_minium_version)
        SEND2(send_headers(), handle_send, _1, send_headers::command);
}

// Send get_blocks sequence.
//-----------------------------------------------------------------------------

// This is fired by the callback (i.e. base timer and stop handler).
void protocol_block::send_get_blocks(const code& ec)
{
    if (stopped())
        return;

    if (ec && ec != error::channel_timeout)
    {
        log::debug(LOG_NODE)
            << "Failure in block timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    blockchain_.fetch_block_locator(
        BIND2(handle_fetch_block_locator, _1, _2));
}

// Peer may respond with headers message if version > 70012.
void protocol_block::handle_fetch_block_locator(const code& ec,
    const hash_list& locator)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure generating block locator for ["
            << authority() << "] " << ec.message();
        return;
    }

    // TODO: use member state to manage the stop hash (see v2).
    const auto stop_hash = null_hash;

    // get_blocks
    const get_blocks request{ std::move(locator), stop_hash };
    SEND2(request, handle_send, _1, request.command);
}

// Receive send_headers sequence.
//-----------------------------------------------------------------------------

bool protocol_block::handle_receive_send_headers(const code& ec,
    message::send_headers::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting " << message->command << " from ["
            << authority() << "] " << ec.message();
        stop(ec);
        return false;
    }

    ////if (bc::protocol_version < send_headers_minium_version)
    ////{
    ////    log::debug(LOG_NODE)
    ////        << "Unsupportable send_headers request by [" << authority() << "]";
    ////    return false;
    ////}

    // send_headers->nop
    send_headers_.store(true);

    // Do not resubscribe after receiving this one-time message.
    return false;
}

// Receive headers sequence.
//-----------------------------------------------------------------------------

// Response to get_blocks message (version >= 70012u).
bool protocol_block::handle_receive_headers(const code& ec,
    message::headers::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting headers from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TODO: request missing blocks.
    // If we do not have the block hash place header in orphan pool.
    // Do not request block until ready to reorganize the orphan pool.
    ///////////////////////////////////////////////////////////////////////////
    //// send_get_data(const code& ec, const hash_list& hashes);

    // headers->get_data[blocks]
    return true;
}

// Receive inventory sequence.
//-----------------------------------------------------------------------------

// Nothing to validate except that we don't have the blocks.
// We should generally be here only if our peer's version is < 70012.
bool protocol_block::handle_receive_inventory(const code& ec,
    message::inventory::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting inventory from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    hash_list block_hashes;
    message->reduce(block_hashes, inventory_type_id::block);

    // TODO: implement orphan_pool_.fetch_missing_block_hashes(...)
    handle_fetch_missing_orphans(error::success, block_hashes);
    return true;
}

void protocol_block::handle_fetch_missing_orphans(const code& ec,
    const hash_list& block_hashes)
{
    if (stopped() || ec == error::service_stopped || block_hashes.empty())
        return;

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating missing orphan hashes for ["
            << authority() << "] " << ec.message();
        return;
    }

    blockchain_.fetch_missing_block_hashes(block_hashes,
        BIND3(send_get_data, _1, _2, inventory_type_id::block));
}

void protocol_block::send_get_data(const code& ec, const hash_list& hashes,
    inventory_type_id type_id)
{
    if (stopped() || ec == error::service_stopped || hashes.empty())
        return;

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating missing block hashes for ["
            << authority() << "] " << ec.message();
        return;
    }

    // inventory->get_data[blocks]
    get_data request(hashes, type_id);
    SEND2(request, handle_send, _1, request.command);
}

// Receive get_data sequence.
//-----------------------------------------------------------------------------

bool protocol_block::handle_receive_get_data(const code& ec,
    message::get_data::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting inventory from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    // Ignore non-block inventory requests in this protocol.
    for (const auto inventory: message->inventories)
        if (inventory.type == inventory_type_id::block)
            blockchain_.fetch_block(inventory.hash,
                BIND3(send_block, _1, _2, inventory.hash));
        else if (inventory.type == inventory_type_id::filtered_block)
            blockchain_.fetch_merkle_block(inventory.hash,
                BIND3(send_merkle_block, _1, _2, inventory.hash));

    return true;
}

void protocol_block::send_block(const code& ec, chain::block::ptr block,
    const hash_digest& hash)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_NODE)
            << "Block requested by [" << authority() << "] not found.";

        const not_found reply{ { inventory_type_id::block, hash } };
        SEND2(reply, handle_send, _1, reply.command);
        return;
    }

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating block requested by ["
            << authority() << "] " << ec.message();
        return;
    }

    SEND2(*block, handle_send, _1, block->command);
}

void protocol_block::send_merkle_block(const code& ec,
    message::merkle_block::ptr merkle, const hash_digest& hash)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_NODE)
            << "Merkle block requested by [" << authority() << "] not found.";

        const not_found reply{ { inventory_type_id::filtered_block, hash } };
        SEND2(reply, handle_send, _1, reply.command);
        return;
    }

    if (ec)
    {
        log::warning(LOG_NODE)
            << "Internal failure locating merkle block requested by ["
            << authority() << "] " << ec.message();
        return;
    }

    SEND2(*merkle, handle_send, _1, merkle->command);
}

// Receive block sequence.
//-----------------------------------------------------------------------------

bool protocol_block::handle_receive_block(const code& ec,
    message::block::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting block from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    blockchain_.store(message, BIND1(handle_store_block, _1));
    return true;
}

void protocol_block::handle_store_block(const code& ec)
{
    if (stopped() || ec == error::service_stopped)
        return;

    // Ignore the block that we already have.
    if (ec == error::duplicate)
    {
        log::debug(LOG_NODE)
            << "Redundant block from [" << authority() << "] "
            << ec.message();
        return;
    }

    // Drop the channel if the block is invalid.
    if (ec)
    {
        log::warning(LOG_NODE)
            << "Invalid block from [" << authority() << "] "
            << ec.message();
        set_event(ec);
        return;
    }

    // The block is accepted as an orphan.
    // There is a DoS vector in peer repeatedly sending the same valid block.
    // We should drop channels that send "large" blocks we haven't requested.
    // We can then announce "small" blocks in place of sending header/inv.
    log::debug(LOG_NODE)
        << "Potential block from [" << authority() << "].";
}

// Other.
//-----------------------------------------------------------------------------

// Handle all sends from one method, since all send failures cause stop.
void protocol_block::handle_send(const code& ec, const std::string& command)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure sending " << command << " to [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }
}

bool protocol_block::handle_reorganized(const code& ec, size_t fork_point,
    const block::ptr_list& incoming, const block::ptr_list& outgoing)
{
    // TODO: send new get_blocks request (see v2).
    return true;
}

} // namespace node
} // namespace libbitcoin
