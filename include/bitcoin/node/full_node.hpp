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
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/sessions.hpp>

namespace libbitcoin {
namespace node {

/// Thread safe.
class BCN_API full_node
  : public network::net
{
public:
    using store = node::store;
    using query = node::query;
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

    /// Validate and organize an unproven header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a proven header, milestone set if in milestone branch.
    virtual void organize(const system::chain::header::cptr& header,
        bool milestone, organize_handler&& handler) NOEXCEPT;
    /// Validate and organize an unproven header announced by compact block.
    virtual void organize_compact(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a submitted (proven) header, prioritized (a tie wins).
    virtual void organize_submitted(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Reorganize to the branch of an archived block of at least equal work.
    virtual void prioritize(const system::hash_digest& hash,
        organize_handler&& handler) NOEXCEPT;

    /// Work a proven header branch must reach (candidate or configured).
    virtual void get_minimum_work(work_handler&& handler) NOEXCEPT;

    /// Validate and archive submitted set, admitted prefix by prefix until one
    /// tx fails. Set is only validated when test, nothing archived.
    virtual void submit(const system::chain::transactions_cptr& txs,
        bool test, submit_handler&& handler) NOEXCEPT;

    /// Validate and archive announced set, admitted prefix by prefix, tx not
    /// accepted passed over (first such failure reported).
    virtual void submit_announced(
        const system::chain::transactions_cptr& txs, size_t slot,
        submit_handler&& handler) NOEXCEPT;

    /// Validate, archive and associate txs of identified compact block, in
    /// block order, txs filling terminal links.
    virtual void submit_compact(const system::chain::transactions_cptr& txs,
        const database::tx_links& links, const database::header_link& link,
        submit_handler&& handler) NOEXCEPT;

    /// Archive and associate txs of identified block, on calling thread (true)
    /// unless pooling, then on strand of compact blocks.
    virtual bool archive_block(code& ec,
        const system::chain::view::block::cptr& block,
        const database::header_link& link, bool strong, bool bypass,
        size_t height, bool prune, result_handler&& handler) NOEXCEPT;

    /// Build a template of pooled txs, parents first, and its height.
    virtual void build_template(template_handler&& handler) NOEXCEPT;

    /// Subscribe to pool scans (notified on the transaction strand).
    virtual void subscribe_pool(pool_notifier&& handler) NOEXCEPT;

    /// Count of changes to the pool (thread safe).
    virtual uint64_t pool_generation() const NOEXCEPT;

    /// The template held, built first if the chain or pool has moved.
    virtual void get_template(block_template_handler&& handler) NOEXCEPT;

    /// Count a demand for template replacement by interval (or uncount).
    virtual void demand_template(bool demand) NOEXCEPT;

    /// Sequence of the template held, zero if none yet (thread safe).
    virtual uint64_t template_generation() const NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        result_handler&& handler) NOEXCEPT;
    virtual void get_unfilled(header_t link, map_handler&& handler) NOEXCEPT;

    /// Reserve or release a high bandwidth compact peer slot.
    virtual bool reserve_compact() NOEXCEPT;
    virtual void release_compact() NOEXCEPT;

    /// Reserve (relay_slots if none), activate or release a relay slot.
    virtual size_t reserve_relay(relay_tier tier) NOEXCEPT;
    virtual void activate_relay(size_t slot, object_key key) NOEXCEPT;
    virtual void release_relay(size_t slot) NOEXCEPT;

    /// Set the fee rate of the relay slot's peer.
    virtual void set_relay_rate(size_t slot, uint64_t rate) NOEXCEPT;

    /// Mark the pooled txs as held by the relay slot's peer.
    virtual void set_relayed(size_t slot,
        const database::tx_links& links) NOEXCEPT;

    /// Relay to the slot's peer the pooled txs it lacks that clear its rate.
    virtual void relay_pool(size_t slot) NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Set chaser event.
    virtual void notify(const code& ec, event_value value) NOEXCEPT;

    /// Set chaser event for the given subscriber only.
    virtual void notify_one(object_key key, const code& ec,
        event_value value) NOEXCEPT;

    /// Call from chaser start() methods (requires strand).
    virtual object_key subscribe_chase(event_notifier&& handler) NOEXCEPT;

    /// Call from protocol start() methods.
    virtual void subscribe_chase(event_notifier&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_chase(object_key key) NOEXCEPT;

    /// Suspensions.
    /// -----------------------------------------------------------------------

    /// Resume nework connections.
    bool resume() NOEXCEPT override;

    /// Suspend all existing and future network connections.
    /// Race condition can result in unsuspended connection.
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

    /// Configuration for all libraries.
    virtual const node::configuration& node_config() const NOEXCEPT;

    /// Configuration settings for all libraries.
    virtual const system::settings& system_settings() const NOEXCEPT;
    virtual const database::settings& database_settings() const NOEXCEPT;
    ////const network::settings& network_settings() const NOEXCEPT override;
    virtual const node::settings& node_settings() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current_chain(bool confirmed) const NOEXCEPT;

    /// The specified timestamp is current.
    virtual bool is_current_time(uint32_t timestamp) const NOEXCEPT;

    /// Header's timestamp is within configured span from current time.
    virtual bool is_current_header(
        const database::header_link& link) const NOEXCEPT;

    /// The confirmed chain is confirmed to maximum height or is current.
    virtual bool is_recent() const NOEXCEPT;

    /// Zulu time at which the node started.
    virtual time_t start_time() const NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Lowest rate (per virtual kilobyte) confirmed by the target block.
    void estimate(size_t target, estimate_mode mode,
        estimate_handler&& handler) NOEXCEPT;

    /// Tx links of the confirmable txs, in pool order.
    void get_confirmable_txs(confirmable_txs_handler&& handler) NOEXCEPT;

    /// Fee rate tuples of the confirmable txs, in pool order.
    void get_confirmable_fees(confirmable_fees_handler&& handler) NOEXCEPT;

    /// Handle performance, base returns false (implied terminate).
    virtual void performance(object_key channel, uint64_t speed,
        result_handler&& handler) NOEXCEPT;

protected:
    /// Session attachments.
    /// -----------------------------------------------------------------------

    /// Override base net to attach derived peer sessions.
    network::session_manual::ptr attach_manual_session() NOEXCEPT override;
    network::session_inbound::ptr attach_inbound_session() NOEXCEPT override;
    network::session_outbound::ptr attach_outbound_session() NOEXCEPT override;

    /// Virtual handlers.
    /// -----------------------------------------------------------------------
    void do_start(const result_handler& handler) NOEXCEPT override;
    void do_run(const result_handler& handler) NOEXCEPT override;
    void do_close() NOEXCEPT override;

private:
    void do_subscribe_chase(const event_notifier& handler,
        const event_completer& complete) NOEXCEPT;
    void do_notify(const code& ec, event_value value) NOEXCEPT;
    void do_notify_one(object_key key, const code& ec,
        event_value value) NOEXCEPT;

    // These are thread safe.
    const configuration& config_;
    const time_t start_time_;
    query& query_;
    std::atomic<size_t> compact_peers_{};

    // These are protected by strand.
    chaser_header chaser_header_;
    chaser_download chaser_download_;
    chaser_validate chaser_validate_;
    chaser_confirm chaser_confirm_;
    chaser_transaction chaser_transaction_;
    chaser_template chaser_template_;
    chaser_snapshot chaser_snapshot_;
    chaser_storage chaser_storage_;
    event_subscriber event_subscriber_{};
};

} // namespace node
} // namespace libbitcoin

#endif
