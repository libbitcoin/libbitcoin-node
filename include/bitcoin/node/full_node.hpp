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
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/configuration.hpp>

namespace libbitcoin {
namespace node {

/// Thread safe.
class BCN_API full_node
  : public network::p2p
{
public:
    using store = node::store;
    using query = node::query;
    typedef std::shared_ptr<full_node> ptr;

    /// Constructors.
    /// -----------------------------------------------------------------------

    full_node(query& query, const configuration& configuration,
        const network::logger& log) NOEXCEPT;

    ~full_node() NOEXCEPT;

    /// Sequences.
    /// -----------------------------------------------------------------------

    /// Start the node (seed and manual services).
    void start(network::result_handler&& handler) NOEXCEPT override;

    /// Run the node (inbound/outbound services and blockchain chasers).
    void run(network::result_handler&& handler) NOEXCEPT override;

    /// Close the node.
    void close() NOEXCEPT override;

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

    /// Call from chaser start() methods (requires strand).
    virtual object_key subscribe_events(event_notifier&& handler) NOEXCEPT;

    /// Call from protocol start() methods.
    virtual void subscribe_events(event_notifier&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Set chaser event.
    virtual void notify(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Set chaser event for the given subscriber only.
    virtual void notify_one(object_key key, const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_events(object_key key) NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Suspend all existing and future network connections.
    /// A race condition could result in an unsuspended connection.
    code suspend(const code& ec) NOEXCEPT override;

    /// Resume nework connections.
    void resume() NOEXCEPT override;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    virtual query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    virtual const configuration& config() const NOEXCEPT;

    /// The candidate chain is current.
    virtual bool is_current() const NOEXCEPT;

    /// The specified timestamp is current.
    virtual bool is_current(uint32_t timestamp) const NOEXCEPT;

protected:
    /// Session attachments.
    /// -----------------------------------------------------------------------
    network::session_manual::ptr attach_manual_session() NOEXCEPT override;
    network::session_inbound::ptr attach_inbound_session() NOEXCEPT override;
    network::session_outbound::ptr attach_outbound_session() NOEXCEPT override;

    /// Virtual handlers.
    /// -----------------------------------------------------------------------
    void do_start(const network::result_handler& handler) NOEXCEPT override;
    void do_run(const network::result_handler& handler) NOEXCEPT override;
    void do_close() NOEXCEPT override;

private:
    object_key create_key() NOEXCEPT;
    void do_subscribe_events(const event_notifier& handler,
        const event_completer& complete) NOEXCEPT;
    void do_notify(const code& ec, chase event_, event_value value) NOEXCEPT;
    void do_notify_one(object_key key, const code& ec, chase event_,
        event_value value) NOEXCEPT;

    // These are thread safe.
    const configuration& config_;
    query& query_;

    // These are protected by strand.
    chaser_block chaser_block_;
    chaser_header chaser_header_;
    chaser_check chaser_check_;
    chaser_preconfirm chaser_preconfirm_;
    chaser_confirm chaser_confirm_;
    chaser_transaction chaser_transaction_;
    chaser_template chaser_template_;
    event_subscriber event_subscriber_;
    object_key keys_{};
};

} // namespace node
} // namespace libbitcoin

#endif
