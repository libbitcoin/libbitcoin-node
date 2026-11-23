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
#ifndef LIBBITCOIN_NODE_TYPES_BLOCK_TEMPLATE_HPP
#define LIBBITCOIN_NODE_TYPES_BLOCK_TEMPLATE_HPP

#include <memory>
#include <bitcoin/node/chase.hpp>

namespace libbitcoin {
namespace node {

/// Template of the next block, its txs parents first.
struct block_template
{
    typedef std::shared_ptr<const block_template> cptr;

    /// Hash of the previous block.
    system::hash_digest previous;

    /// Context of the block, as the pool validates under it.
    system::chain::context context;

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

    /// Ids, totals and parent positions of the txs.
    database::template_txs txs;
};

} // namespace node
} // namespace libbitcoin

#endif
