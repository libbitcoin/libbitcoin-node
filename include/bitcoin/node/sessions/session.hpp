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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {
    
/// Common session context.
class BCN_API session
{
public:
    DELETE_COPY_MOVE(session);

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(uint64_t channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT;

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        chaser::organize_handler&& handler) NOEXCEPT;

    /// Organize a validated block.
    virtual void organize(const system::chain::block::cptr& block,
        chaser::organize_handler&& handler) NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(chaser_check::handler&& handler) NOEXCEPT;
    virtual void put_hashes(const chaser_check::map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Set chaser event (does not require network strand).
    virtual void notify(const code& ec, chaser::chase event_,
        chaser::link value) NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    full_node::query& archive() const NOEXCEPT;

protected:

    /// Construct/destruct the session.
    session(full_node& node) NOEXCEPT;

    /// Asserts that session is stopped.
    ~session() NOEXCEPT;

private:
    // This is thread safe (mostly).
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
