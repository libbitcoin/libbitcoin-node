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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_HPP

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/estimator.hpp>

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

    /// Reorganize to the branch of an archived block of at least equal work.
    virtual void prioritize(const system::hash_digest& hash,
        organize_handler&& handler) NOEXCEPT;

    /// Validate and archive a submitted package, accepted as a whole.
    /// The package is only validated when test, so nothing is archived.
    virtual void submit(const system::chain::transactions_cptr& txs,
        bool test, submit_handler&& handler) NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Set a chaser event.
    virtual void notify(const code& ec, event_value value) const NOEXCEPT;

    /// Set chaser event for the given subscriber only.
    virtual void notify_one(object_key key, const code& ec,
        event_value value) const NOEXCEPT;

    /// Subscribe to chaser events (requires node strand).
    virtual object_key subscribe_chase(event_notifier&& handler) NOEXCEPT;

    /// Subscribe to chaser events.
    virtual void subscribe_chase(event_notifier&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_chase(object_key key) NOEXCEPT;

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

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(object_key channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Network suspension (does not affect administrative connections).
    virtual bool suspended() const NOEXCEPT;
    virtual void suspend(const code& ec) NOEXCEPT;
    virtual bool resume() NOEXCEPT;

    /// Suspend all connections.
    virtual void fault(const code& ec) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    node::query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    virtual const node::configuration& node_config() const NOEXCEPT;
    virtual const system::settings& system_settings() const NOEXCEPT;
    virtual const database::settings& database_settings() const NOEXCEPT;
    ////const network::settings& network_settings() const NOEXCEPT override;
    virtual const node::settings& node_settings() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current_chain(bool confirmed) const NOEXCEPT;

    /// The confirmed chain is confirmed to maximum height or is current.
    virtual bool is_recent() const NOEXCEPT;

    /// Zulu time at which the node started.
    virtual time_t start_time() const NOEXCEPT;

    /// The number of peer channels.
    virtual size_t channel_count() const NOEXCEPT;

    /// The number of inbound peer channels.
    virtual size_t inbound_channel_count() const NOEXCEPT;

    /// The number of host pool addresses.
    virtual size_t address_count() const NOEXCEPT;


protected:

    /// Constructors.
    /// -----------------------------------------------------------------------

    session(full_node& node) NOEXCEPT;

private:
    void do_subscribe_chase(const event_notifier& handler,
        const event_completer& complete) NOEXCEPT;

private:
    // This is thread safe (mostly).
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
