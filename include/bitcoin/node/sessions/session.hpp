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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_HPP

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

 class full_node;

/// Common session context, presumes will be joined with network::session.
class BCN_API session
{
public:
    typedef std::shared_ptr<session> ptr;

    DELETE_COPY_MOVE_DESTRUCT(session);

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
        event_value value) const NOEXCEPT;

    /// Set chaser event for the given subscriber only.
    virtual void notify_one(object_key key, const code& ec, chase event_,
        event_value value) const NOEXCEPT;

    /// Subscribe to chaser events (requires node strand).
    virtual object_key subscribe_events(event_notifier&& handler) NOEXCEPT;

    /// Subscribe to chaser events.
    virtual void subscribe_events(event_notifier&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_events(object_key key) NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(object_key channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT;

    /// Get the memory resource.
    virtual network::memory& get_memory() const NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Suspend all connections.
    virtual void fault(const code& ec) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    node::query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current(bool confirmed) const NOEXCEPT;

    /// The confirmed chain is confirmed to maximum height or is current.
    virtual bool is_recent() const NOEXCEPT;

protected:

    /// Constructors.
    /// -----------------------------------------------------------------------

    session(full_node& node) NOEXCEPT;

private:
    void do_subscribe_events(const event_notifier& handler,
        const event_completer& complete) NOEXCEPT;

private:
    // This is thread safe (mostly).
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
