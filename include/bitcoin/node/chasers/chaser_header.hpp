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

#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger header branches for the candidate chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'header' event.
class BCN_API chaser_header
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_header);

    chaser_header(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    virtual code start() NOEXCEPT;

    /// Validate and organize next header in sequence relative to caller peer.
    virtual void organize(const system::chain::header::cptr& header_ptr,
        organize_handler&& handler) NOEXCEPT;

protected:
    virtual void handle_event(const code& ec, chase event_, link value) NOEXCEPT;
    virtual void do_disorganize(header_t height) NOEXCEPT;
    virtual void do_organize(const system::chain::header::cptr& header,
        const organize_handler& handler) NOEXCEPT;

private:
    struct header_state
    {
        system::chain::header::cptr header;
        system::chain::chain_state::ptr state;
    };
    using header_tree = std::unordered_map<system::hash_digest, header_state>;
    using header_links = std::vector<database::header_link>;

    system::chain::chain_state::ptr get_chain_state(
        const system::hash_digest& hash) const NOEXCEPT;
    bool get_branch_work(uint256_t& work, size_t& branch_point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT;
    bool get_is_strong(bool& strong, const uint256_t& work,
        size_t branch_point) const NOEXCEPT;
    void cache(const system::chain::header::cptr& header,
        const system::chain::chain_state::ptr& state) NOEXCEPT;
    database::header_link push_header(
        const system::chain::header::cptr& header,
        const system::chain::context& context) const NOEXCEPT;
    bool push_header(const system::hash_digest& key) NOEXCEPT;

    // This is thread safe.
    const system::settings& settings_;

    // These are protected by strand.
    system::chain::chain_state::ptr state_{};
    header_tree tree_{};
};

} // namespace node
} // namespace libbitcoin

#endif
