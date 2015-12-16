/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/session.hpp>

#include <cstddef>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/inventory.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>

namespace libbitcoin {
namespace node {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using namespace bc::chain;
using namespace bc::network;

// Start protocol, bind poller/responder/inventory to to new channels.
// Subscribe to reorgs to announce new blocks to peers.
// TODO: we are not announcing new transactions across channels.
session::session(protocol& protocol, blockchain& blockchain,
    transaction_pool& tx_pool, poller& poller, responder& responder,
    inventory& inventory, size_t minimum_start_height)
  : protocol_(protocol),
    blockchain_(blockchain),
    tx_pool_(tx_pool),
    poller_(poller),
    responder_(responder),
    inventory_(inventory),
    minimum_start_height_(minimum_start_height)
{
}

void session::start(completion_handler handle_complete)
{
    protocol_.start(
        std::bind(&session::handle_start,
            this, _1, handle_complete));
}

void session::stop(completion_handler handle_complete)
{
    protocol_.stop(handle_complete);
}

void session::handle_start(const std::error_code& ec,
    completion_handler handle_complete)
{
    if (ec)
    {
        log_error(LOG_SESSION)
            << "Failure starting session: " << ec.message();
        handle_complete(ec);
        return;
    }

    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&session::new_channel,
            this, _1, _2));

    // Subscribe to new reorganizations.
    blockchain_.subscribe_reorganize(
        std::bind(&session::broadcast_new_blocks,
            this, _1, _2, _3, _4));

    handle_complete(ec);
}

bool session::new_channel(const std::error_code& ec, channel_ptr node)
{
    if (ec == error::service_stopped)
        return false;

    // Poll this channel to build the blockchain.
    poller_.monitor(node);

    // Respond to get_data and get_blocks messages on this channel.
    responder_.monitor(node);

    // Respond to inventory messages on this channel, requesting needed data.
    inventory_.monitor(node);
    return true;
}

bool session::broadcast_new_blocks(const std::error_code& ec,
    uint32_t fork_point, const blockchain::block_list& new_blocks,
    const blockchain::block_list& /* replaced_blocks */)
{
    if (ec == error::service_stopped)
        return false;

    if (ec)
    {
        log_error(LOG_SESSION)
            << "Failure in reorganize: " << ec.message();
        return false;
    }

    // Don't bother publishing blocks when in the initial blockchain download.
    if (fork_point < minimum_start_height_)
        return true;

    // Broadcast new blocks inventory.
    inventory_type blocks_inventory;
    for (const auto block: new_blocks)
    {
        const inventory_vector_type inventory
        {
            inventory_type_id::block,
            hash_block_header(block->header)
        };

        blocks_inventory.inventories.push_back(inventory);
    }

    log_debug(LOG_SESSION)
        << "Broadcasting block inventory [" 
        << blocks_inventory.inventories.size() << "]";

    const auto broadcast_handler = [](const std::error_code& ec, size_t count)
    {
        if (ec)
            log_debug(LOG_SESSION)
                << "Failure broadcasting block inventory: " << ec.message();
        else
            log_debug(LOG_SESSION)
                << "Broadcast block inventory to (" << count << ") nodes.";
    };

    // TODO: optimize by not broadcasting to the node from which it came, by
    // maintaining each peer's last known checkpoint.
    protocol_.broadcast(blocks_inventory, broadcast_handler);
    return true;
}

} // namespace node
} // namespace libbitcoin
