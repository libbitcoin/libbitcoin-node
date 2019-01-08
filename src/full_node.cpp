/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/full_node.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session_inbound.hpp>
#include <bitcoin/node/sessions/session_manual.hpp>
#include <bitcoin/node/sessions/session_outbound.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::network;
using namespace bc::system;
using namespace bc::system::chain;
using namespace bc::system::config;
using namespace boost::adaptors;
using namespace std::placeholders;

full_node::full_node(const configuration& configuration)
  : p2p(configuration.network),
    reservations_(configuration.network.minimum_connections(),
        configuration.node.maximum_deviation,
        configuration.node.block_latency_seconds),
    chain_(thread_pool(), configuration.chain, configuration.database,
        configuration.bitcoin),
    protocol_maximum_(configuration.network.protocol_maximum),
    chain_settings_(configuration.chain),
    node_settings_(configuration.node)
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

    if (!chain_.start())
    {
        LOG_ERROR(LOG_NODE)
            << "Failure starting blockchain.";
        handler(error::operation_failed);
        return;
    }

    // This is invoked on the same thread.
    // Stopped is true and no network threads until after this call.
    p2p::start(handler);
}

// Run sequence.
// ----------------------------------------------------------------------------

// This follows seeding as an explicit step, sync hooks may go here.
void full_node::run(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    handle_running(error::success, handler);
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
        LOG_ERROR(LOG_NODE)
            << "Failure synchronizing blocks: " << ec.message();
        handler(ec);
        return;
    }

    checkpoint top_confirmed;
    if (!chain_.get_top(top_confirmed, false))
    {
        LOG_ERROR(LOG_NODE)
            << "The confirmed chain is corrupt.";
        handler(error::operation_failed);
        return;
    }

    set_top_block(top_confirmed);
    LOG_INFO(LOG_NODE)
        << "Top confirmed block height is (" << top_confirmed.height() << ").";

    checkpoint top_candidate;
    if (!chain_.get_top(top_candidate, true))
    {
        LOG_ERROR(LOG_NODE)
            << "The candidate chain is corrupt.";
        handler(error::operation_failed);
        return;
    }

    subscribe_headers(
        std::bind(&full_node::handle_reindexed,
            this, _1, _2, _3, _4));

    subscribe_blocks(
        std::bind(&full_node::handle_reorganized,
            this, _1, _2, _3, _4));

    set_top_header(top_candidate);
    const auto top_candidate_height = top_candidate.height();

    LOG_INFO(LOG_NODE)
        << "Top candidate block height is (" << top_candidate_height << ").";

    hash_digest hash;
    const auto top_valid_candidate_height =
        chain_.top_valid_candidate_state()->height();

    LOG_INFO(LOG_NODE)
        << "Top valid candidate block height (" << top_valid_candidate_height
        << ").";

    // Prime download queue.
    for (auto height = top_candidate_height;
        height > top_valid_candidate_height; --height)
        if (chain_.get_downloadable(hash, height))
            reservations_.push_front(std::move(hash), height);

    LOG_INFO(LOG_NODE)
        << "Pending candidate block downloads (" << reservations_.size()
        << ").";

    // Prime validator.
    const auto next_validatable_height = top_valid_candidate_height + 1u;
    if (chain_.get_validatable(hash, next_validatable_height))
    {
        LOG_INFO(LOG_NODE)
            << "Next candidate pending validation (" << next_validatable_height
            << ").";

        chain_.prime_validation(hash, next_validatable_height);
    }

    // This is invoked on a new thread.
    // This is the end of the derived run startup sequence.
    p2p::run(handler);
}

// A typical reorganization consists of one incoming and zero outgoing blocks.
bool full_node::handle_reindexed(code ec, size_t fork_height,
    header_const_ptr_list_const_ptr incoming,
    header_const_ptr_list_const_ptr outgoing)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure handling reindex: " << ec.message();
        stop();
        return false;
    }

    // Nothing to do here.
    if (!incoming || incoming->empty())
        return true;

    // First pop height is highest outgoing.
    auto height = fork_height + outgoing->size();

    // Pop outgoing reservations from download queue (if at top), high first.
    for (const auto header: reverse(*outgoing))
        reservations_.pop_back(*header, height--);

    // Push unpopulated incoming reservations (can't expect parent), low first.
    for (const auto header: *incoming)
        reservations_.push_back(*header, ++height);

    // Top height will be: fork_height + incoming->size();
    set_top_header({ incoming->back()->hash(), height });
    return true;
}

// A typical reorganization consists of one incoming and zero outgoing blocks.
bool full_node::handle_reorganized(code ec, size_t fork_height,
    block_const_ptr_list_const_ptr incoming,
    block_const_ptr_list_const_ptr outgoing)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure handling reorganization: " << ec.message();
        stop();
        return false;
    }

    // Nothing to do here.
    if (!incoming || incoming->empty())
        return true;

    auto height = fork_height + outgoing->size();

    // No stats for unconfirmation.
    for (const auto block: reverse(*outgoing))
    {
        LOG_INFO(LOG_NODE)
            << "Unconfirmed #" << height-- << " ["
            << encode_hash(block->hash()) << "]";
    }

    // Only log every 100th validated block, until current or unless outgoing.
    const auto period = chain_.is_validated_stale() &&
       outgoing->empty() ? 100u : 1u;

    for (const auto block: *incoming)
        if ((++height % period) == 0)
            report(*block, height);

    const auto top_height = fork_height + incoming->size();
    set_top_block({ incoming->back()->hash(), top_height });
    return true;
}

template<typename Type>
float to_float(const asio::duration& time)
{
    const auto count =  std::chrono::duration_cast<Type>(time).count();
    return static_cast<float>(count);
}

template<typename Type>
size_t to_ratio(const asio::duration& time, size_t value)
{
    return static_cast<size_t>(std::round(to_float<Type>(time) / value));
}

// static
void full_node::report(const chain::block& block, size_t height)
{
    static const auto form = "Valid  #%06i [%s] "
        "%|4i| txs %|4i| ins %|3i| des %|3i| pop "
        "%|3i| acc %|3i| scr %|3i| can %|3i| con %|f|";

    const auto& times = block.metadata;
    const auto transactions = block.transactions().size();
    const auto inputs = std::max(block.total_inputs(), size_t(1));

    LOG_INFO(LOG_NODE)
        << boost::format(form) %
            height %
            encode_hash(block.hash()) %
            transactions %
            inputs %

            // query total (des)
            to_ratio<asio::microseconds>(block.metadata.deserialize, inputs) %

            // populate total (pop)
            to_ratio<asio::microseconds>(block.metadata.populate, inputs) %

            // accept total (acc)
            to_ratio<asio::microseconds>(block.metadata.accept, inputs) %

            // script total (scr)
            to_ratio<asio::microseconds>(block.metadata.connect, inputs) %

            // candidate total (can)
            to_ratio<asio::microseconds>(block.metadata.candidate, inputs) %

            // confirm total (con)
            to_ratio<asio::microseconds>(block.metadata.confirm, inputs) %

            // this block transaction cache efficiency in hits/queries
            block.metadata.cache_efficiency;
}

// Specializations.
// ----------------------------------------------------------------------------
// Create derived sessions and override these to inject from derived node.

// Must not connect until running, otherwise messages may conflict with sync.
// But we establish the session in network so caller doesn't need to run.
network::session_manual::ptr full_node::attach_manual_session()
{
    return attach<node::session_manual>(chain_);
}

network::session_inbound::ptr full_node::attach_inbound_session()
{
    return attach<node::session_inbound>(chain_);
}

network::session_outbound::ptr full_node::attach_outbound_session()
{
    return attach<node::session_outbound>(chain_);
}

// Shutdown
// ----------------------------------------------------------------------------

bool full_node::stop()
{
    // Suspend new work last so we can use work to clear subscribers.
    const auto p2p_stop = p2p::stop();
    const auto chain_stop = chain_.stop();

    if (!p2p_stop)
        LOG_ERROR(LOG_NODE)
            << "Failed to stop network.";

    if (!chain_stop)
        LOG_ERROR(LOG_NODE)
            << "Failed to stop blockchain.";

    return p2p_stop && chain_stop;
}

// This must be called from the thread that constructed this class (see join).
bool full_node::close()
{
    // Invoke own stop to signal work suspension.
    if (!full_node::stop())
        return false;

    const auto p2p_close = p2p::close();
    const auto chain_close = chain_.close();

    if (!p2p_close)
        LOG_ERROR(LOG_NODE)
            << "Failed to close network.";

    if (!chain_close)
        LOG_ERROR(LOG_NODE)
            << "Failed to close blockchain.";

    return p2p_close && chain_close;
}

// Properties.
// ----------------------------------------------------------------------------

const node::settings& full_node::node_settings() const
{
    return node_settings_;
}

const blockchain::settings& full_node::chain_settings() const
{
    return chain_settings_;
}

safe_chain& full_node::chain()
{
    return chain_;
}

// Downloader.
// ----------------------------------------------------------------------------

reservation::ptr full_node::get_reservation()
{
    return reservations_.get();
}

size_t full_node::download_queue_size() const
{
    return reservations_.size();
}

// Subscriptions.
// ----------------------------------------------------------------------------

void full_node::subscribe_blocks(block_handler&& handler)
{
    chain().subscribe_blocks(std::move(handler));
}

void full_node::subscribe_headers(header_handler&& handler)
{
    chain().subscribe_headers(std::move(handler));
}

void full_node::subscribe_transactions(transaction_handler&& handler)
{
    chain().subscribe_transactions(std::move(handler));
}

} // namespace node
} // namespace libbitcoin
