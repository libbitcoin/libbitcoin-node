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

#include <functional>
#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger header branches for the candidate chain.
/// Weak branches are retained in a hash table unless already store populated.
/// Strong branches reorganize the candidate chain and fire the 'header' event.
class BCN_API chaser_header
  : public chaser
{
public:
    chaser_header(full_node& node) NOEXCEPT;

    virtual bool start() NOEXCEPT;

    /// Organize the next header in sequence, relative to caller's peer.
    /// Causes a fault/stop if preceding headers have not been stored.
    /// Caller must validate the header and provide context.
    virtual void organize(const system::chain::header::cptr& header,
        system::chain::context&& context) NOEXCEPT;

protected:
    typedef std::vector<database::header_link> header_links;

    /// Handlers.
    virtual void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT;

    /// Sum of work from header to branch point (excluded).
    virtual bool get_branch_work(uint256_t& work, size_t& point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT;

    /// Strong if new branch work exceeds candidate work.
    /// Also obtains branch point for work summation termination.
    /// Also obtains ordered branch identifiers for subsequent reorg.
    virtual bool get_is_strong(bool& strong, const uint256_t& work,
        size_t point) const NOEXCEPT;

    /// Header timestamp is within configured span from current time.
    virtual bool is_current(const system::chain::header& header) const NOEXCEPT;

    /// Save header to tree with validation context.
    virtual void save(const system::chain::header::cptr& header,
        const system::chain::context& context) NOEXCEPT;

    /// Store header to database and push to top of candidate chain.
    virtual database::header_link push(
        const system::chain::header::cptr& header,
        const system::chain::context& context) const NOEXCEPT;

    /// Move tree header to database and push to top of candidate chain.
    virtual bool push(const system::hash_digest& key) NOEXCEPT;

private:
    struct proposed_header
    {
        database::context context;
        system::chain::header::cptr header;
    };

    void do_handle_event(const code& ec, chase event_, link value) NOEXCEPT;
    void do_organize(const system::chain::header::cptr& header,
        const system::chain::context& context) NOEXCEPT;

    // These are protected by strand.
    std::unordered_map<system::hash_digest, proposed_header> tree_{};
    const network::wall_clock::duration currency_window_;
    const bool use_currency_window_;
};

} // namespace node
} // namespace libbitcoin

#endif
