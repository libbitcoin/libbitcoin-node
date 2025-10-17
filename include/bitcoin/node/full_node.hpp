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
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <bitcoin/node/block_memory.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/sessions.hpp>

namespace libbitcoin {
namespace node {

/// Thread safe.
/// WARNING: when full node is using block_memory controller, all shared block
/// components invalidate when the block destructs. Lifetime of the block is
/// assured for the extent of all methods below, however if a sub-object is
/// retained by shared_ptr, beyond method completion, a copy of the block
/// shared_ptr must also be be retained. Taking a block or sub-object copy is
/// insufficient, as copies are shallow (copy internal shared_ptr objects).
class BCN_API full_node
  : public network::net
{
public:
    using store = node::store;
    using query = node::query;
    using memory_controller = block_memory;
    using result_handler = network::result_handler;
    typedef std::shared_ptr<full_node> ptr;

    /// Constructors.
    /// -----------------------------------------------------------------------

    full_node(query& query, const configuration& configuration,
        const network::logger& log) NOEXCEPT;

    ~full_node() NOEXCEPT;

    /// Sequences.
    /// -----------------------------------------------------------------------

    /// Start the node (seed and manual services).
    void start(result_handler&& handler) NOEXCEPT override;

    /// Run the node (inbound/outbound services and blockchain chasers).
    void run(result_handler&& handler) NOEXCEPT override;

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
        result_handler&& handler) NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Set chaser event.
    virtual void notify(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Set chaser event for the given subscriber only.
    virtual void notify_one(object_key key, const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Call from chaser start() methods (requires strand).
    virtual object_key subscribe_events(event_notifier&& handler) NOEXCEPT;

    /// Call from protocol start() methods.
    virtual void subscribe_events(event_notifier&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_events(object_key key) NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Resume nework connections.
    void resume() NOEXCEPT override;

    /// Suspend all existing and future network connections.
    /// A race condition can result in an unsuspended connection.
    void suspend(const code& ec) NOEXCEPT override;

    /// Handle a fault that has occurred.
    virtual void fault(const code& ec) NOEXCEPT;

    /// Prune the store, suspends and resumes network.
    virtual code prune(const store::event_handler& handler) NOEXCEPT;

    /// Snapshot the store, suspends and resumes network.
    virtual code snapshot(const store::event_handler& handler) NOEXCEPT;

    /// Reset store disk full condition.
    virtual code reload(const store::event_handler& handler) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    virtual query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    virtual const configuration& config() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current(bool confirmed) const NOEXCEPT;

    /// The specified timestamp is current.
    virtual bool is_current(uint32_t timestamp) const NOEXCEPT;

    /// The confirmed chain is confirmed to maximum height or is current.
    virtual bool is_recent() const NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(object_key channel, uint64_t speed,
        result_handler&& handler) NOEXCEPT;

    /// Get the memory resource.
    virtual network::memory& get_memory() NOEXCEPT;

protected:
    /// Session attachments.
    /// -----------------------------------------------------------------------

    /// Override base net to attach derived peer sessions.
    network::session_manual::ptr attach_manual_session() NOEXCEPT override;
    network::session_inbound::ptr attach_inbound_session() NOEXCEPT override;
    network::session_outbound::ptr attach_outbound_session() NOEXCEPT override;

    /// Attach server sessions (base net doesn't specialize or start these).
    virtual session_web::ptr attach_web_session() NOEXCEPT;
    virtual session_explore::ptr attach_explore_session() NOEXCEPT;
    virtual session_websocket::ptr attach_websocket_session() NOEXCEPT;
    virtual session_bitcoind::ptr attach_bitcoind_session() NOEXCEPT;
    virtual session_electrum::ptr attach_electrum_session() NOEXCEPT;
    virtual session_stratum_v1::ptr attach_stratum_v1_session() NOEXCEPT;
    virtual session_stratum_v2::ptr attach_stratum_v2_session() NOEXCEPT;

    /// Virtual handlers.
    /// -----------------------------------------------------------------------
    void do_start(const result_handler& handler) NOEXCEPT override;
    void do_run(const result_handler& handler) NOEXCEPT override;
    void do_close() NOEXCEPT override;

private:
    void start_web(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_explore(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_websocket(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_bitcoind(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_electrum(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_stratum_v1(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_stratum_v2(const code& ec, const result_handler& handler) NOEXCEPT;

    void do_subscribe_events(const event_notifier& handler,
        const event_completer& complete) NOEXCEPT;
    void do_notify(const code& ec, chase event_, event_value value) NOEXCEPT;
    void do_notify_one(object_key key, const code& ec, chase event_,
        event_value value) NOEXCEPT;

    // These are thread safe.
    const configuration& config_;
    memory_controller memory_;
    query& query_;

    // These are protected by strand.
    chaser_block chaser_block_;
    chaser_header chaser_header_;
    chaser_check chaser_check_;
    chaser_validate chaser_validate_;
    chaser_confirm chaser_confirm_;
    chaser_transaction chaser_transaction_;
    chaser_template chaser_template_;
    chaser_snapshot chaser_snapshot_;
    chaser_storage chaser_storage_;
    event_subscriber event_subscriber_;
};

} // namespace node
} // namespace libbitcoin

#endif
