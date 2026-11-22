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

    /// Validate and archive a submitted set, admitted prefix by prefix until
    /// one tx fails. The set is only validated when test, nothing archived.
    virtual void submit(const system::chain::transactions_cptr& txs,
        bool test, submit_handler&& handler) NOEXCEPT;

    /// Validate and archive an announced set, admitted prefix by prefix, a tx
    /// that is not accepted passed over (the first such failure reported).
    virtual void submit_announced(
        const system::chain::transactions_cptr& txs, size_t slot,
        submit_handler&& handler) NOEXCEPT;

    /// Validate, archive and associate the txs of an identified compact block,
    /// in block order, the txs filling the terminal links.
    virtual void submit_compact(const system::chain::transactions_cptr& txs,
        const database::tx_links& links, const database::header_link& link,
        submit_handler&& handler) NOEXCEPT;

    /// Archive and associate the txs of an identified block, on the calling
    /// thread (true) unless pooling, then on the strand of compact blocks.
    virtual bool archive_block(code& ec,
        const system::chain::view::block::cptr& block,
        const database::header_link& link, bool strong, bool bypass,
        size_t height, bool prune, network::result_handler&& handler) NOEXCEPT;

    /// Work a proven header branch must reach (candidate or configured).
    virtual void get_minimum_work(work_handler&& handler) NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;
    virtual void get_unfilled(header_t link, map_handler&& handler) NOEXCEPT;

    /// Reserve or release a high bandwidth compact peer slot.
    virtual bool reserve_compact() NOEXCEPT;
    virtual void release_compact() NOEXCEPT;

    /// The memory pool of the session's peers is requested once relaying.
    virtual bool request_mempool() const NOEXCEPT;

    /// The relay slot tier of the session's peers.
    virtual relay_tier relay_tier_() const NOEXCEPT;

    /// Reserve (relay_slots if none), activate or release a relay slot.
    virtual size_t reserve_relay() NOEXCEPT;
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

    /// A template of pooled txs built upon the request, parents first, and
    /// its height.
    virtual void get_template(template_handler&& handler) NOEXCEPT;

    /// Lowest rate (per virtual kilobyte) confirmed by the target block.
    void estimate(size_t target, estimate_mode mode,
        estimate_handler&& handler) NOEXCEPT;

    /// Tx links of the confirmable txs, in pool order.
    void get_confirmable_txs(confirmable_txs_handler&& handler) NOEXCEPT;

    /// Fee rate tuples of the confirmable txs, in pool order.
    void get_confirmable_fees(confirmable_fees_handler&& handler) NOEXCEPT;

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
    virtual bool is_current_time(uint32_t timestamp) const NOEXCEPT;

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
