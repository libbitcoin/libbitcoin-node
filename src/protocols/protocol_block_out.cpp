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
#include <bitcoin/node/protocols/protocol_block_out.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block"
#define CLASS protocol_block_out

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

static constexpr auto locator_cap = 500u;
static constexpr auto send_headers_version = 70012u;

protocol_block_out::protocol_block_out(p2p& network, channel::ptr channel,
    block_chain& blockchain)
  : protocol_events(network, channel, NAME),
    threshold_(null_hash),
    blockchain_(blockchain),
    headers_to_peer_(false),
    CONSTRUCT_TRACK(protocol_block_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    if (bc::protocol_version >= send_headers_version)
    {
        // Send headers vs. inventory anncements if headers_to_peer_ is set.
        SUBSCRIBE2(send_headers, handle_receive_send_headers, _1, _2);
    }

    SUBSCRIBE2(get_headers, handle_receive_get_headers, _1, _2);
    SUBSCRIBE2(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);

    // Subscribe to block acceptance notifications (our heartbeat).
    blockchain_.subscribe_reorganize(
        BIND4(handle_reorganized, _1, _2, _3, _4));
}

// Receive send_headers.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_send_headers(const code& ec,
    send_headers_ptr message)
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

    // Block annoucements will be headers messages instead of inventory.
    headers_to_peer_.store(true);

    // Do not resubscribe after handling this one-time message.
    return false;
}

// Receive get_headers sequence.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_get_headers(const code& ec,
    get_headers_ptr message)
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

    ///////////////////////////////////////////////////////////////////////////
    // TODO: manage the locator threashold for this peer (see v2).
    ///////////////////////////////////////////////////////////////////////////

    blockchain_.fetch_locator_block_headers(*message, threshold_, locator_cap,
        BIND2(handle_fetch_locator_headers, _1, _2));
    return true;
}

void protocol_block_out::handle_fetch_locator_headers(const code& ec,
    const header_list& headers)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating locator block headers for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // Respond to get_headers with headers.
    const message::headers response(headers);
    SEND2(response, handle_send, _1, response.command);
}

// Receive get_blocks sequence.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_get_blocks(const code& ec,
    get_blocks_ptr message)
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

    ///////////////////////////////////////////////////////////////////////////
    // TODO: manage the locator threashold for this peer (see v2).
    ///////////////////////////////////////////////////////////////////////////

    blockchain_.fetch_locator_block_hashes(*message, threshold_, locator_cap,
        BIND2(handle_fetch_locator_hashes, _1, _2));
    return true;
}

void protocol_block_out::handle_fetch_locator_hashes(const code& ec,
    const hash_list& hashes)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating locator block hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // Respond to get_blocks with inventory.
    const inventory response(hashes, inventory_type_id::block);
    SEND2(response, handle_send, _1, response.command);
}

// Receive get_data sequence.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_get_data(const code& ec,
    get_data_ptr message)
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

    ////// TODO: revise blockchain to accept block_message type.
    ////// Ignore non-block inventory requests in this protocol.
    ////for (const auto& inventory: message->inventories)
    ////    if (inventory.type == inventory_type_id::block)
    ////        blockchain_.fetch_block(inventory.hash,
    ////            BIND3(send_block, _1, _2, inventory.hash));
    ////    else if (inventory.type == inventory_type_id::filtered_block)
    ////        blockchain_.fetch_merkle_block(inventory.hash,
    ////            BIND3(send_merkle_block, _1, _2, inventory.hash));

    return true;
}

void protocol_block_out::send_block(const code& ec, block_ptr block,
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
        log::error(LOG_NODE)
            << "Internal failure locating block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*block, handle_send, _1, block->command);
}

void protocol_block_out::send_merkle_block(const code& ec,
    merkle_block_ptr merkle, const hash_digest& hash)
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
        log::error(LOG_NODE)
            << "Internal failure locating merkle block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*merkle, handle_send, _1, merkle->command);
}

// Subscription.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_reorganized(const code& ec, size_t fork_point,
    const block_ptr_list& incoming, const block_ptr_list& outgoing)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: add host id to message subscriber to avoid block reflection.
    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    // TODO: send new get_blocks request (see v2).
    // Select response via headers or inventory using headers_to_peer_.
    ///////////////////////////////////////////////////////////////////////////
    return true;
}

void protocol_block_out::handle_stop(const code&)
{
    log::debug(LOG_NETWORK)
        << "Stopped block_out protocol";
}

} // namespace node
} // namespace libbitcoin
