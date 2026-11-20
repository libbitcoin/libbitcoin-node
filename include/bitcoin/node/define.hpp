/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_NODE_DEFINE_HPP
#define LIBBITCOIN_NODE_DEFINE_HPP

/// Standard includes (do not include directly).

/// Pulls in common /node headers (excluding settings/config/full_node).
#include <bitcoin/node/chase.hpp>

/// For common types below.
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

/// Alias system code.
typedef std::error_code code;

/// Estimate types.
enum class estimate_mode
{
    /// The pool as it is, with no arrivals.
    basic,

    /// Arrivals weighted by recency, each block half of the one above it.
    geometric,

    /// Arrivals at their mean over the economical window.
    economical,

    /// Arrivals at their peak over the conservative window.
    conservative,

    unknown
};
constexpr size_t maximum_estimate_horizon = 1008;

/// Blocks of arrivals (two hours and one day at ten minutes per block).
constexpr size_t economical_estimate_window = 12;
constexpr size_t conservative_estimate_window = 144;
typedef std::function<void(const code&, uint64_t)> estimate_handler;

/// Organization types.
typedef std::function<void(const code&, const uint256_t&)> work_handler;
typedef std::function<void(const code&, size_t)> organize_handler;
typedef std::function<void(const code&, size_t)> submit_handler;
typedef database::store<database::mmap> store;
typedef database::query<store> query;

/// Work types.
typedef network::race_all<const code&> job;
typedef std::shared_ptr<database::associations> map_ptr;
typedef std::function<void(const code&, const map_ptr&,
    const job::ptr&)> map_handler;

/// bip152: high bandwidth mode is requested of at most three peers.
constexpr size_t maximum_compact_peers = 3;

/// Mining types.
typedef std::function<void(const code&, size_t,
    const database::tx_links&)> template_handler;

/// Confirmable types.
typedef std::function<void(const code&,
    const database::tx_links&)> confirmable_txs_handler;
typedef std::function<void(const code&,
    const database::fee_rates&)> confirmable_fees_handler;

/// Event desubscriber.
typedef network::desubscriber<object_key, event_value> event_subscriber;
typedef event_subscriber::handler event_notifier;
typedef event_subscriber::completer event_completer;

/// Scans of the pool.
enum class pool_scan : uint8_t
{
    /// Coverage of an incoming set.
    coverage,

    /// Index write of an admitted tx.
    admission,

    /// Index transition of an organized block.
    block_in,

    /// Index transition of a reorganized block.
    block_out,

    /// Index sweep of the pool context.
    context,

    /// Template.
    template_,

    /// Fee projection.
    projection,

    /// Confirmable txs or fees.
    confirmable
};

/// A scan of the pool and what it did.
struct pool_event
{
    pool_scan scan{};
    system::pool::report_t report{};
};

/// Pool unsubscriber.
typedef network::unsubscriber<pool_event> pool_subscriber;
typedef pool_subscriber::handler pool_notifier;

// Inventory messages.
using type_id = network::messages::peer::inventory_item::type_id;

} // namespace node
} // namespace libbitcoin

#endif
