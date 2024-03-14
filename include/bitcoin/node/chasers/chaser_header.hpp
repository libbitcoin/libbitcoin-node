/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_HEADER_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_HEADER_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser_organize.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger header branches for the candidate chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'header' event.
class BCN_API chaser_header
  : public chaser_organize<system::chain::header>
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_header);

    chaser_header(full_node& node) NOEXCEPT;

protected:
    /// Get header from Block instance.
    virtual const system::chain::header& get_header(
        const system::chain::header& header) const NOEXCEPT;

    /// Query store for const pointer to Block instance.
    virtual bool get_block(system::chain::header::cptr& out,
        size_t index) const NOEXCEPT;

    /// Determine if Block is valid.
    virtual code validate(const system::chain::header& header,
        const system::chain::chain_state& state) const NOEXCEPT;

    /// Determine if Block is top of a storable branch.
    virtual bool is_storable(const system::chain::header& header,
        size_t height, const system::hash_digest& hash,
        const system::chain::chain_state& state) const NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
