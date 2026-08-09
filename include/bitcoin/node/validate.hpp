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
#ifndef LIBBITCOIN_NODE_VALIDATE_HPP
#define LIBBITCOIN_NODE_VALIDATE_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

/// Transaction validation against the confirmed chain, for services that
/// accept transactions from clients (there is no tx pool until v5). Shared
/// by the server protocols, which obtain the context of the next block.

/// Validate tx in the given context, populating its prevout metadata.
/// Includes the block-rule guards, a DoS guard for standalone validation.
BCN_API code validate_transaction(const system::chain::transaction& tx,
    const query& query, const system::chain::context& pool) NOEXCEPT;

} // namespace node
} // namespace libbitcoin

#endif
