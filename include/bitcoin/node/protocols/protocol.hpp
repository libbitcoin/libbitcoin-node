/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

// Only session.hpp.
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols, thread safe.
/// This node::protocol is not derived from network::protocol, but given the
/// channel constructor parameter is derived from network::channel, the strand
/// is accessible despite lack of bind/post/parallel templates access. This
/// allows event subscription by derived protocols without the need to derive
/// from protocol_peer (which would prevent derivation from service protocols).
class BCN_API protocol
{
protected:
    DELETE_COPY_MOVE_DESTRUCT(protocol);

    /// Constructors.
    /// -----------------------------------------------------------------------

    // reinterpret_pointer_cast because channel is abstract.
    inline protocol(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : channel_(channel), session_(session)
    {
    }

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    virtual const node::configuration& node_config() const NOEXCEPT;
    virtual const system::settings& system_settings() const NOEXCEPT;
    virtual const database::settings& database_settings() const NOEXCEPT;
    ////const network::settings& network_settings() const NOEXCEPT override;
    virtual const node::settings& node_settings() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current(bool confirmed) const NOEXCEPT;

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

private:
    void handle_subscribed(const code& ec, object_key key) NOEXCEPT;
    void handle_subscribe(const code& ec, object_key key,
        const event_completer& complete) NOEXCEPT;

    // This channel requires stranded calls, base is thread safe.
    const network::channel::ptr channel_;

    // This is thread safe.
    const node::session::ptr session_;

    // This is protected by singular subscription.
    object_key key_{};
};

} // namespace node
} // namespace libbitcoin

#endif
