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
#include <bitcoin/node/full_node.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session_block_sync.hpp>
#include <bitcoin/node/sessions/session_header_sync.hpp>
#include <bitcoin/node/sessions/session_inbound.hpp>
#include <bitcoin/node/sessions/session_manual.hpp>
#include <bitcoin/node/sessions/session_outbound.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::config;
using namespace bc::network;
using namespace std::placeholders;

full_node::full_node(const configuration& configuration)
  : p2p(configuration.network),
    hashes_(configuration.chain.checkpoints),
    blockchain_(thread_pool(), configuration.chain, configuration.database),
    protocol_maximum_(configuration.network.protocol_maximum),
    settings_(configuration.node)
{
}

full_node::~full_node()
{
    full_node::close();
}

// Start.
// ----------------------------------------------------------------------------

void full_node::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    if (!blockchain_.start())
    {
        log::error(LOG_NODE)
            << "Blockchain failed to start.";
        handler(error::operation_failed);
        return;
    }

    // This is invoked on the same thread.
    // Stopped is true and no network threads until after this call.
    p2p::start(handler);
}

// Run sequence.
// ----------------------------------------------------------------------------

void full_node::run(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // TODO: make this safe by requiring sync if gaps found.
    ////// By setting no download connections checkpoints can be used without sync.
    ////// This also allows the maximum protocol version to be set below headers.
    ////if (settings_.download_connections == 0)
    ////{
    ////    // This will spawn a new thread before returning.
    ////    handle_running(error::success, handler);
    ////    return;
    ////}

    // The instance is retained by the stop handler (i.e. until shutdown).
    const auto header_sync = attach_header_sync_session();

    // This is invoked on a new thread.
    header_sync->start(
        std::bind(&full_node::handle_headers_synchronized,
            this, _1, handler));
}

void full_node::handle_headers_synchronized(const code& ec,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure synchronizing headers: " << ec.message();
        handler(ec);
        return;
    }

    // The instance is retained by the stop handler (i.e. until shutdown).
    const auto block_sync = attach_block_sync_session();

    // This is invoked on a new thread.
    block_sync->start(
        std::bind(&full_node::handle_running,
            this, _1, handler));
}

void full_node::handle_running(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure synchronizing blocks: " << ec.message();
        handler(ec);
        return;
    }

    size_t height;

    if (!blockchain_.get_last_height(height))
    {
        log::error(LOG_NODE)
            << "The blockchain is corrupt.";
        handler(error::operation_failed);
        return;
    }

    set_top_block({ null_hash, height });

    log::info(LOG_NODE)
        << "Node start height is (" << height << ").";

    subscribe_blockchain(
        std::bind(&full_node::handle_reorganized,
            this, _1, _2, _3, _4));

    // This is invoked on a new thread.
    // This is the end of the derived run startup sequence.
    p2p::run(handler);
}

// A typical reorganization consists of one incoming and zero outgoing blocks.
bool full_node::handle_reorganized(const code& ec, size_t fork_height,
    const block_const_ptr_list& incoming, const block_const_ptr_list& outgoing)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure handling reorganization: " << ec.message();
        stop();
        return false;
    }

    for (const auto block: outgoing)
        log::debug(LOG_NODE)
            << "Reorganization moved block to orphan pool ["
            << encode_hash(block->header.hash()) << "]";

    BITCOIN_ASSERT(!incoming.empty());
    const auto height = safe_add(fork_height, incoming.size());

    set_top_block({ incoming.back()->hash(), height });
    return true;
}

// Specializations.
// ----------------------------------------------------------------------------
// Create derived sessions and override these to inject from derived node.

// Must not connect until running, otherwise imports may conflict with sync.
// But we establish the session in network so caller doesn't need to run.
network::session_manual::ptr full_node::attach_manual_session()
{
    return attach<node::session_manual>(blockchain_);
}

network::session_inbound::ptr full_node::attach_inbound_session()
{
    return attach<node::session_inbound>(blockchain_);
}

network::session_outbound::ptr full_node::attach_outbound_session()
{
    return attach<node::session_outbound>(blockchain_);
}

session_header_sync::ptr full_node::attach_header_sync_session()
{
    const auto& checkpoints = blockchain_.chain_settings().checkpoints;
    return attach<session_header_sync>(hashes_, blockchain_, checkpoints);
}

session_block_sync::ptr full_node::attach_block_sync_session()
{
    return attach<session_block_sync>(hashes_, blockchain_, settings_);
}

// Shutdown
// ----------------------------------------------------------------------------

bool full_node::stop()
{
    // Suspend new work last so we can use work to clear subscribers.
    const auto p2p_stop = p2p::stop();
    const auto chain_stop = blockchain_.stop();

    if (!p2p_stop)
        log::error(LOG_NODE)
            << "Failed to stop network.";

    if (!chain_stop)
        log::error(LOG_NODE)
            << "Failed to stop database.";

    return p2p_stop && chain_stop;
}

// This must be called from the thread that constructed this class (see join).
bool full_node::close()
{
    // Invoke own stop to signal work suspension.
    if (!full_node::stop())
        return false;

    const auto p2p_close = p2p::close();
    const auto chain_close = blockchain_.close();

    if (!p2p_close)
        log::error(LOG_NODE)
            << "Failed to close network.";

    if (!chain_close)
        log::error(LOG_NODE)
            << "Failed to close database.";

    return p2p_close && chain_close;
}

// Properties.
// ----------------------------------------------------------------------------

const settings& full_node::node_settings() const
{
    return settings_;
}

full_chain& full_node::chain()
{
    return blockchain_;
}

// Subscriptions.
// ----------------------------------------------------------------------------

void full_node::subscribe_blockchain(reorganize_handler handler)
{
    chain().subscribe_reorganize(handler);
}

void full_node::subscribe_transaction(transaction_handler handler)
{
    chain().subscribe_transaction(handler);
}

} // namespace node
} //namespace libbitcoin
