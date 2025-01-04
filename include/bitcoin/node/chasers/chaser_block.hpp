/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_BLOCK_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_BLOCK_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser_organize.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger block branches for the confirmed chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'connect' event.
class BCN_API chaser_block
  : public chaser_organize<system::chain::block>
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_block);

    chaser_block(full_node& node) NOEXCEPT;

protected:
    /// Get header from Block instance.
    const system::chain::header& get_header(
        const system::chain::block& block) const NOEXCEPT override;

    /// Query store for const pointer to Block instance by candidate height.
    bool get_block(system::chain::block::cptr& out,
        size_t height) const NOEXCEPT override;

    /// Determine if Block is a duplicate (success for not duplicate).
    code duplicate(size_t& height,
        const system::hash_digest& hash) const NOEXCEPT override;

    /// Determine if Block is valid.
    code validate(const system::chain::block& block,
        const chain_state& state) const NOEXCEPT override;

    /// Determine if state is top of a storable branch (always true).
    bool is_storable(const chain_state& state) const NOEXCEPT override;

    /// True if Block is on a milestone-covered branch.
    bool is_under_milestone(size_t height) const NOEXCEPT override;

    /// Milestone tracking.
    void update_milestone(const system::chain::header& header,
        size_t height, size_t branch_point) NOEXCEPT override;

private:
    void set_prevout(const system::chain::input& input) const NOEXCEPT;
    void populate(const system::chain::block& block) const NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
