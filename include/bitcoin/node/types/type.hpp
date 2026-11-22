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
#ifndef LIBBITCOIN_NODE_TYPES_TYPE_HPP
#define LIBBITCOIN_NODE_TYPES_TYPE_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/chase.hpp>
#include <bitcoin/node/types/pool_event.hpp>

namespace libbitcoin {
namespace node {

/// Estimate types.
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

/// Mining types.
typedef std::function<void(const code&, size_t,
    const database::tx_links&)> template_handler;

/// Template of the next block, its txs parents first.
struct block_template
{
    typedef std::shared_ptr<const block_template> cptr;

    /// Height of the block.
    size_t height;

    /// Count of templates held to this one.
    uint64_t sequence;

    /// Sum of the fees of the txs.
    uint64_t fees;

    /// The txs, less the coinbase.
    database::tx_links links;

    /// Witness commitment of the coinbase, its reserved value null.
    system::hash_digest commitment;

    /// Merkle branch of the coinbase.
    system::hashes branch;
};

typedef std::function<void(const code&,
    const block_template::cptr&)> block_template_handler;

/// Confirmable types.
typedef std::function<void(const code&,
    const database::tx_links&)> confirmable_txs_handler;
typedef std::function<void(const code&,
    const database::fee_rates&)> confirmable_fees_handler;

/// Event desubscriber.
typedef network::desubscriber<object_key, event_value> event_subscriber;
typedef event_subscriber::handler event_notifier;
typedef event_subscriber::completer event_completer;

/// Pool unsubscriber.
typedef network::unsubscriber<pool_event> pool_subscriber;
typedef pool_subscriber::handler pool_notifier;

/// Inventory messages.
using type_id = network::messages::peer::inventory_item::type_id;

} // namespace node
} // namespace libbitcoin

#endif
