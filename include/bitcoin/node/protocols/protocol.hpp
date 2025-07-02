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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP

 // Individual session.hpp inclusion to prevent cycle (can't forward declare).
#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/channel.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols, thread safe.
class BCN_API protocol
  : public network::protocol
{
protected:
    /// Constructors.
    /// -----------------------------------------------------------------------
    /// static_pointer_cast relies on create_channel().

    // Need template (see session member)?
    template <typename SessionPtr>
    protocol(const SessionPtr& session,
        const network::channel::ptr& channel) NOEXCEPT
      : network::protocol(session, channel), session_(session),
        channel_(std::static_pointer_cast<node::channel>(channel))
    {
    }

    virtual ~protocol() NOEXCEPT;

    /// Organizers.
    /// -----------------------------------------------------------------------

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a checked block.
    virtual void organize(const system::chain::block::cptr& block,
        organize_handler&& handler) NOEXCEPT;

    /// Get block hashes for blocks to download.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;

    /// Submit block hashes for blocks not downloaded.
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Report performance, handler may direct self-terminate.
    virtual void performance(uint64_t speed,
        network::result_handler&& handler) const NOEXCEPT;

    /// Suspend all existing and future network connections.
    /// A race condition could result in an unsuspended connection.
    virtual code fault(const code& ec) NOEXCEPT;

    /// Announcements.
    /// -----------------------------------------------------------------------

    /// Set an incoming block or tx hash that peer announced.
    virtual void set_announced(const system::hash_digest&) NOEXCEPT;

    /// Determine if outgoing block or tx was previously announced by peer.
    virtual bool was_announced(const system::hash_digest&) const NOEXCEPT;

    /// Events notification.
    /// -----------------------------------------------------------------------

    /// Set a chaser event.
    virtual void notify(const code& ec, chase event_,
        event_value value) const NOEXCEPT;

    /// Set a chaser event.
    virtual void notify_one(object_key key, const code& ec, chase event_,
        event_value value) const NOEXCEPT;

    /// Events subscription.
    /// -----------------------------------------------------------------------

    /// Subscribe to chaser events (max one active per protocol).
    virtual void subscribe_events(event_notifier&& handler) NOEXCEPT;

    /// Override to handle subscription completion (stranded).
    virtual void subscribed(const code& ec, object_key key) NOEXCEPT;

    /// Unsubscribe from chaser events.
    /// Subscribing protocol must invoke from overridden stopping().
    virtual void unsubscribe_events() NOEXCEPT;

    /// Get the subscription key (for notify_one).
    virtual object_key events_key() const NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current(bool confirmed) const NOEXCEPT;

private:
    void handle_subscribed(const code& ec, object_key key) NOEXCEPT;
    void handle_subscribe(const code& ec, object_key key,
        const event_completer& complete) NOEXCEPT;

    // This is thread safe.
    const session::ptr session_;

    // This derived channel requires stranded calls, base is thread safe.
    node::channel::ptr channel_;

    // This is protected by singular subscription.
    object_key key_{};
};

} // namespace node
} // namespace libbitcoin

#endif
