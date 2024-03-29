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
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {
    
/// Common session context, presumes will be joined with network::session.
/// This could be templatized on the sibling, but there only one implemented.
class BCN_API session
{
public:
    DELETE_COPY_MOVE(session);

    /// Organizers.
    /// -----------------------------------------------------------------------

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a validated block.
    virtual void organize(const system::chain::block::cptr& block,
        organize_handler&& handler) NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Set a chaser event.
    virtual void notify(const code& ec, chase event_,
        event_link value) NOEXCEPT;

    /// Subscribe to chaser events.
    virtual void async_subscribe_events(event_handler&& handler) NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(uint64_t channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// The candidate chain is current.
    virtual bool is_current() const NOEXCEPT;

protected:
    /// Constructors.
    /// -----------------------------------------------------------------------

    session(full_node& node) NOEXCEPT;
    ~session() NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Subscribe to chaser events (requires strand).
    virtual void subscribe_events(const event_handler& handler) NOEXCEPT;

private:
    // This is thread safe (mostly).
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
