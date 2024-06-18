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
    virtual const system::chain::header& get_header(
        const system::chain::block& block) const NOEXCEPT;

    /// Query store for const pointer to Block instance by candidate height.
    virtual bool get_block(system::chain::block::cptr& out,
        size_t height) const NOEXCEPT;

    /// Determine if Block is valid.
    virtual code validate(const system::chain::block& block,
        const chain_state& state) const NOEXCEPT;

    /// Notify check chaser to redownload the block (nop).
    virtual void do_malleated(header_t link) NOEXCEPT;

    /// Determine if state is top of a storable branch (always true).
    virtual bool is_storable(const chain_state& state) const NOEXCEPT;

    /// True if Block is on a milestone-covered branch.
    virtual bool is_under_milestone(size_t height) const NOEXCEPT;

    /// Milestone tracking.
    virtual void update_milestone(const system::chain::header& header,
        size_t height, size_t branch_point) NOEXCEPT;

private:
    void set_prevout(const system::chain::input& input) const NOEXCEPT;
    void populate(const system::chain::block& block) const NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
