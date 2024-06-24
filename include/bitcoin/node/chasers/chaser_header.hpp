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

    /// Initialize chaser state.
    code start() NOEXCEPT override;

protected:
    /// Get header from Block instance.
    virtual const system::chain::header& get_header(
        const system::chain::header& header) const NOEXCEPT;

    /// Query store for const pointer to Block instance by candidate height.
    virtual bool get_block(system::chain::header::cptr& out,
        size_t height) const NOEXCEPT;

    /// Determine if Block is a duplicate (success for not duplicate).
    virtual code duplicate(size_t& height,
        const system::hash_digest& hash) const NOEXCEPT;

    /// Determine if Block is valid.
    virtual code validate(const system::chain::header& header,
        const chain_state& state) const NOEXCEPT;

    /// Notify check chaser to redownload the block.
    virtual void do_malleated(header_t link) NOEXCEPT;

    /// Determine if state is top of a storable branch.
    virtual bool is_storable(const chain_state& state) const NOEXCEPT;

    /// True if Block is on a milestone-covered branch.
    virtual bool is_under_milestone(size_t height) const NOEXCEPT;

    /// Milestone tracking.
    virtual void update_milestone(const system::chain::header& header,
        size_t height, size_t branch_point) NOEXCEPT;

private:
    bool is_checkpoint(const chain_state& state) const NOEXCEPT;
    bool is_milestone(const chain_state& state) const NOEXCEPT;
    bool is_current(const chain_state& state) const NOEXCEPT;
    bool is_hard(const chain_state& state) const NOEXCEPT;
    bool initialize_milestone() NOEXCEPT;

    // This is thread safe.
    const system::chain::checkpoint& milestone_;

    // This is protected by strand.
    size_t active_milestone_height_{};
};

} // namespace node
} // namespace libbitcoin

#endif
