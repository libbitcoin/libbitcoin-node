/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/estimator.hpp>

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
    virtual bool is_current_chain(bool confirmed) const NOEXCEPT;

    /// The minimum fee rate (satoshis/kvB) to relay, max_money if not pooling.
    virtual uint64_t minimum_fee_rate() const NOEXCEPT;

    /// Zulu time at which the node started.
    virtual time_t start_time() const NOEXCEPT;

    /// The number of peer channels.
    virtual size_t channel_count() const NOEXCEPT;

    /// The number of inbound peer channels.
    virtual size_t inbound_channel_count() const NOEXCEPT;

    /// The number of host pool addresses.
    virtual size_t address_count() const NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Maintain a manual connection to the given endpoint.
    virtual void connect(const network::config::endpoint& endpoint) NOEXCEPT;

    /// Connect to the given endpoint, handler invoked on each connect/stop.
    virtual void connect(const network::config::endpoint& endpoint,
        network::net::channel_notifier&& handler) NOEXCEPT;

    /// Get current fee estimate.
    void estimate(size_t target, estimator::mode mode,
        estimate_handler&& handler) NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Network suspension (does not affect administrative connections).
    virtual bool suspended() const NOEXCEPT;
    virtual void suspend(const code& ec) NOEXCEPT;
    virtual bool resume() NOEXCEPT;

    /// Organizers.
    /// -----------------------------------------------------------------------

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a checked block.
    virtual void organize(const system::chain::block::cptr& block,
        organize_handler&& handler) NOEXCEPT;

    /// Reorganize to the branch of an archived block of at least equal work.
    virtual void prioritize(const system::hash_digest& hash,
        organize_handler&& handler) NOEXCEPT;

    /// Validate and archive a submitted package, accepted as a whole.
    /// The package is only validated when test, so nothing is archived.
    virtual void submit(const system::chain::transactions_cptr& txs,
        bool test, submit_handler&& handler) NOEXCEPT;

    /// Events subscription.
    /// -----------------------------------------------------------------------

    /// Subscribe to chaser events (max one active per protocol).
    virtual void subscribe_chase(event_notifier&& handler) NOEXCEPT;

    /// Override to handle subscription completion (stranded).
    virtual void subscribed(const code& ec, object_key key) NOEXCEPT;

    /// Unsubscribe from chaser events.
    /// Subscribing protocol must invoke from overridden stopping().
    virtual void unsubscribe_chase() NOEXCEPT;

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
