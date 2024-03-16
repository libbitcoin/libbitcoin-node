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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_ORGANIZE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_ORGANIZE_HPP

#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Abstract intermediate base class to hold complex and consensus critical
/// common code for blocks-first and headers-first chain organizations.
template<typename Block>
class chaser_organize
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_organize);

    chaser_organize(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    virtual code start() NOEXCEPT;

    /// Validate and organize next block in sequence relative to caller peer.
    virtual void organize(const typename Block::cptr& block_ptr,
        organize_handler&& handler) NOEXCEPT;

protected:
    using chain_state = system::chain::chain_state;
    struct block_state
    {
        typename Block::cptr block;
        chain_state::ptr state;
    };
    using block_tree = std::unordered_map<system::hash_digest, block_state>;
    using header_links = std::vector<database::header_link>;

    /// Pure Virtual
    /// -----------------------------------------------------------------------

    /// Get header from Block instance.
    virtual const system::chain::header& get_header(
        const Block& block) const NOEXCEPT = 0;

    /// Query store for const pointer to Block instance.
    virtual bool get_block(typename Block::cptr& out,
        size_t index) const NOEXCEPT = 0;

    /// Determine if Block is valid.
    virtual code validate(const Block& block,
        const chain_state& state) const NOEXCEPT = 0;

    /// Determine if Block is top of a storable branch.
    virtual bool is_storable(const Block& block,
        const chain_state& state) const NOEXCEPT = 0;

    /// Properties
    /// -----------------------------------------------------------------------

    /// Constant access to Block tree.
    virtual const block_tree& tree() const NOEXCEPT;

    /// System configuration settings.
    virtual const system::settings& settings() const NOEXCEPT;

    /// Methods
    /// -----------------------------------------------------------------------

    /// Handle chaser events.
    virtual void handle_event(const code&, chase event_, link value) NOEXCEPT;

    /// Reorganize following block unconfirmability.
    virtual void do_disorganize(header_t header) NOEXCEPT;

    /// Reorganize following strong branch discovery.
    virtual void do_organize(typename Block::cptr& block_ptr,
        const organize_handler& handler) NOEXCEPT;

private:
    static constexpr auto fork_bits = to_bits(sizeof(system::chain::forks));
    static constexpr bool is_block() NOEXCEPT
    {
        return is_same_type<Block, system::chain::block>;
    }

    // Store Block into logical tree cache.
    void cache(const typename Block::cptr& block_ptr,
        const chain_state::ptr& state) NOEXCEPT;

    // Obtain chain state for given header hash, nullptr if not found. 
    chain_state::ptr get_chain_state(
        const system::hash_digest& hash) const NOEXCEPT;

    // Sum of work from header to branch point (excluded).
    bool get_branch_work(uint256_t& work, size_t& branch_point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT;

    // True if work represents a stronger candidate branch.
    bool get_is_strong(bool& strong, const uint256_t& work,
        size_t branch_point) const NOEXCEPT;

    // Store Block to database and push to top of candidate chain.
    database::header_link push(const typename Block::cptr& block_ptr,
        const system::chain::context& context) const NOEXCEPT;

    // Move tree Block to database and push to top of candidate chain.
    bool push(const system::hash_digest& key) NOEXCEPT;

    // This is thread safe.
    const system::settings& settings_;

    // These are protected by strand.
    chain_state::ptr state_{};
    block_tree tree_{};
};

} // namespace node
} // namespace libbitcoin

#define TEMPLATE template <typename Block>
#define CLASS chaser_organize<Block>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

#include <bitcoin/node/impl/chasers/chaser_organize.ipp>

BC_POP_WARNING()

#undef CLASS

#endif
