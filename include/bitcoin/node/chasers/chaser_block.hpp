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

#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger block branches for the confirmed chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'connect' event.
class BCN_API chaser_block
  : public chaser
{
public:
    DELETE_COPY_MOVE(chaser_block);

    chaser_block(full_node& node) NOEXCEPT;
    virtual ~chaser_block() NOEXCEPT;

    virtual code start() NOEXCEPT;

    /// Validate and organize next block in sequence relative to caller peer.
    /// Causes a fault/stop if preceding blocks have not been stored.
    virtual void organize(
        const system::chain::block::cptr& block_ptr) NOEXCEPT;

protected:
    struct validated_block
    {
        database::context context;
        system::chain::block::cptr item;
    };
    typedef std::vector<database::header_link> header_links;

    // This is protected by strand.
    system::chain::chain_state::ptr state_{};
    std::unordered_map<system::hash_digest, validated_block> tree_{};

    /// Handlers.
    virtual void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT;

    /// Sum of work from header to fork point (excluded).
    virtual bool get_branch_work(uint256_t& work, size_t& point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT;

    /// Strong if new branch work exceeds confirmed work.
    /// Also obtains fork point for work summation termination.
    /// Also obtains ordered branch identifiers for subsequent reorg.
    virtual bool get_is_strong(bool& strong, const uint256_t& work,
        size_t point) const NOEXCEPT;

    /// Header timestamp is within configured span from current time.
    virtual bool is_current(const system::chain::header& header,
        size_t height) const NOEXCEPT;

    /// Save block to tree with validation context.
    virtual void save(const system::chain::block::cptr& block,
        const system::chain::context& context) NOEXCEPT;

    /// Store block to database and push to top of candidate chain.
    virtual database::header_link push(
        const system::chain::block::cptr& block,
        const system::chain::context& context) const NOEXCEPT;

    /// Move tree header to database and push to top of candidate chain.
    virtual bool push(const system::hash_digest& key) NOEXCEPT;

    /// Properties.
    /// Given non-current blocks cached in memory, should always be zero/false.
    virtual const network::wall_clock::duration& currency_window() const NOEXCEPT;
    virtual bool use_currency_window() const NOEXCEPT;

private:
    void do_handle_event(const code& ec, chase event_, link value) NOEXCEPT;
    void do_organize(const system::chain::block::cptr& block) NOEXCEPT;

    // These are thread safe.
    const system::chain::checkpoints& checkpoints_;
    const network::wall_clock::duration currency_window_;
    const bool use_currency_window_;
};

} // namespace node
} // namespace libbitcoin

#endif
