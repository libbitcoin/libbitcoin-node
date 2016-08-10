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

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    headers_to_peer_(bc::protocol_version >= send_headers_version),
    CONSTRUCT_TRACK(protocol_block_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    if (headers_to_peer_)
    {
        // Send headers vs. inventory anncements if headers_to_peer_ is set.
        SUBSCRIBE2(send_headers, handle_receive_send_headers, _1, _2);
    }

    // TODO: move get_headers to a derived class protocol_block_out_31800.
    SUBSCRIBE2(get_headers, handle_receive_get_headers, _1, _2);
    SUBSCRIBE2(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);

    // Subscribe to block acceptance notifications (our heartbeat).
    blockchain_.subscribe_reorganize(
        BIND4(handle_reorganized, _1, _2, _3, _4));
}

// Receive send_headers.
//-----------------------------------------------------------------------------

// TODO: move send_headers to a derived class protocol_block_out_70012.
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

// TODO: move get_headers to a derived class protocol_block_out_31800.
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

// TODO: move headers to a derived class protocol_block_out_31800.
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
    // TODO: manage the locator threshold for this peer (see v2).
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

// TODO: move filtered_block to derived class protocol_block_out_70001.
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

    // TODO: these must return message objects or be copied!
    // Ignore non-block inventory requests in this protocol.
    for (const auto& inventory: message->inventories)
    {
        if (inventory.type == inventory_type_id::block)
            blockchain_.fetch_block(inventory.hash,
                BIND3(send_block, _1, _2, inventory.hash));
        else if (inventory.type == inventory_type_id::filtered_block)
            blockchain_.fetch_merkle_block(inventory.hash,
                BIND3(send_merkle_block, _1, _2, inventory.hash));
    }

    return true;
}

// TODO: move not_found to derived class protocol_block_out_70001.
void protocol_block_out::send_block(const code& ec, chain::block::ptr block,
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

// TODO: move filtered_block to derived class protocol_block_out_70001.
void protocol_block_out::send_merkle_block(const code& ec,
    merkle_block_ptr message, const hash_digest& hash)
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

    SEND2(*message, handle_send, _1, message->command);
}

// Subscription.
//-----------------------------------------------------------------------------

// TODO: make sure we are announcing older blocks first here.
bool protocol_block_out::handle_reorganized(const code& ec, size_t fork_point,
    const block_ptr_list& incoming, const block_ptr_list& outgoing)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure handling reorganization: " << ec.message();
        stop(ec);
        return false;
    }

    // TODO: move announce headers to a derived class protocol_block_in_70012.
    if (headers_to_peer_)
    {
        headers announcement;

        for (const auto block: incoming)
            if (block->originator() != nonce())
                announcement.elements.push_back(block->header);

        if (!announcement.elements.empty())
            SEND2(announcement, handle_send, _1, announcement.command);
        return true;
    }

    static const auto id = inventory_type_id::block;
    inventory announcement;

    for (const auto block: incoming)
        if (block->originator() != nonce())
            announcement.inventories.push_back( { id, block->header.hash() });

    if (!announcement.inventories.empty())
        SEND2(announcement, handle_send, _1, announcement.command);
    return true;
}

void protocol_block_out::handle_stop(const code&)
{
    log::debug(LOG_NETWORK)
        << "Stopped block_out protocol";
}

} // namespace node
} // namespace libbitcoin
