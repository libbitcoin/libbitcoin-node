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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <unordered_set>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down unconfirmed transactions.
class BCN_API chaser_transaction
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_transaction);

    chaser_transaction(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;
    void stop() NOEXCEPT override;

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

    /// Archive and associate the txs of an identified block. Not pooling, on
    /// the calling thread, returning true with the code. Pooling, on the
    /// strand with compact blocks, returning false and handling the code
    /// (thread safe).
    virtual bool archive_block(code& ec,
        const system::chain::view::block::cptr& block,
        const database::header_link& link, bool strong, bool bypass,
        size_t height, bool prune, network::result_handler&& handler) NOEXCEPT;

    /// Build a template of pooled txs, parents first, and its height.
    virtual void build_template(template_handler&& handler) NOEXCEPT;

    /// Lowest rate (per virtual kilobyte) confirmed by the target block.
    virtual void estimate(size_t target, estimate_mode mode,
        estimate_handler&& handler) NOEXCEPT;

    /// Tx links of the confirmable txs, in pool order.
    virtual void get_confirmable_txs(
        confirmable_txs_handler&& handler) NOEXCEPT;

    /// Fee rate tuples of the confirmable txs, in pool order.
    virtual void get_confirmable_fees(
        confirmable_fees_handler&& handler) NOEXCEPT;

    /// Subscribe to pool scans (notified on the strand).
    virtual void subscribe_pool(pool_notifier&& handler) NOEXCEPT;

    /// Reserve a free slot of the tier, relay_slots if none (thread safe).
    virtual size_t reserve_relay(relay_tier tier) NOEXCEPT;

    /// Release the relay slot, free once cleared from the marks.
    virtual void release_relay(size_t slot) NOEXCEPT;

    /// Set the fee rate of the slot's peer, a fall sweeping the pool to it.
    virtual void set_relay_rate(size_t slot, uint64_t rate) NOEXCEPT;

    /// Mark the pooled txs as held by the slot's peer.
    virtual void set_relayed(size_t slot,
        const database::tx_links& links) NOEXCEPT;

    /// Relay to the slot's peer the pooled txs it lacks that clear its rate.
    virtual void relay_pool(size_t slot) NOEXCEPT;

protected:
    using chaser::broadcast;

    virtual bool handle_chase(const code& ec, event_value value) NOEXCEPT;

    virtual void do_submit(const system::chain::transactions_cptr& txs,
        bool test, bool announced, size_t slot,
        const submit_handler& handler) NOEXCEPT;
    virtual void do_submit_compact(
        const system::chain::transactions_cptr& txs,
        const database::tx_links& links, const database::header_link& link,
        const submit_handler& handler) NOEXCEPT;
    virtual void do_archive_block(
        const system::chain::view::block::cptr& block,
        const database::header_link& link, bool strong, bool bypass,
        size_t height, bool prune,
        const network::result_handler& handler) NOEXCEPT;
    virtual void do_build_template(const template_handler& handler) NOEXCEPT;
    virtual void do_estimate(size_t target, estimate_mode mode,
        const estimate_handler& handler) NOEXCEPT;
    virtual void do_get_confirmable_txs(
        const confirmable_txs_handler& handler) NOEXCEPT;
    virtual void do_get_confirmable_fees(
        const confirmable_fees_handler& handler) NOEXCEPT;

    /// Recompute the pool context and sweep the index by it if due, closing
    /// the pool if not current or the context is not reusable, and map scan
    /// starts: built once current, extended by block until pooling, and
    /// remapped when a block archived before the latch is organized.
    virtual void do_bump() NOEXCEPT;

    /// Retain the pool subscription.
    virtual void do_subscribe_pool(const pool_notifier& handler) NOEXCEPT;

    /// Relay slot management.
    virtual void do_release_relay(size_t slot) NOEXCEPT;
    virtual void do_set_relay_rate(size_t slot, uint64_t rate) NOEXCEPT;
    virtual void do_set_relayed(size_t slot,
        const database::tx_links& links) NOEXCEPT;
    virtual void do_relay_pool(size_t slot) NOEXCEPT;

    /// Apply the organized block to the pool index, then bump.
    virtual void do_organized(header_t link) NOEXCEPT;

    /// Announce the current block as settled in the pool.
    virtual void do_block(header_t link) NOEXCEPT;

    /// Apply the reorganized block to the pool index and pool its unpooled txs.
    virtual void do_reorganized(header_t link) NOEXCEPT;

    /// Raise the arrivals floor above the pruned block.
    virtual void do_pruned(header_t link) NOEXCEPT;

    /// Map scan starts from the confirmed top (scan threadpool).
    virtual void do_scan(const database::header_link& top, size_t bound,
        bool remap) NOEXCEPT;

    /// Complete the scan start map.
    virtual void do_scanned(bool mapped) NOEXCEPT;

    /// Revalidate the pooled txs under the context (revalidation threadpool).
    virtual void do_revalidate(const database::tx_links& links,
        const system::chain::context& ctx) NOEXCEPT;

    /// Restore the pooled txs that passed, if the context still holds.
    virtual void do_revalidated(const database::tx_links& links,
        const system::data_chunk& passed,
        const system::chain::context& ctx) NOEXCEPT;

private:
    /// A set under submission, its running set and outcome.
    struct submission
    {
        system::chain::transactions_cptr txs;
        size_t slot;
        uint32_t time;
        bool test;
        bool announced;
        std::vector<bool> stored{};
        std::unordered_set<system::hash_digest> held{};
        query::pool_graph graph{};
        uint64_t bytes{};
        uint64_t sigops{};
        size_t given{};
        size_t first{};
        size_t passed{};
        bool running{};
        bool cleared{};
        code failure{};
    };

    static size_t to_rate(const system::chain::transaction& tx) NOEXCEPT;
    system::pool::algorithm::totals_t block_limit() const NOEXCEPT;

    /// Submission steps.
    void hold(submission& set) const NOEXCEPT;
    bool spends_held(const submission& set,
        const system::chain::transaction& tx) const NOEXCEPT;
    void pass_over(submission& set, size_t index, const code& ec) NOEXCEPT;
    void count(submission& set,
        const system::chain::transaction& tx) const NOEXCEPT;
    code extend(bool& covered, submission& set, size_t index) NOEXCEPT;
    code restore(submission& set, size_t index) NOEXCEPT;
    code clear(size_t& at, submission& set, size_t index) NOEXCEPT;

    code check(const system::chain::transaction_cptrs& txs,
        const std::vector<bool>& stored, bool announced) NOEXCEPT;
    code accept(const system::chain::transaction& tx) NOEXCEPT;
    code admit(database::tx_link& out, const system::chain::transaction& tx,
        uint32_t time) NOEXCEPT;
    code validate(const system::chain::transaction& tx,
        const system::chain::context& ctx) NOEXCEPT;
    database::tx_link to_stored(
        const system::chain::transaction& tx) const NOEXCEPT;
    code fill(database::tx_link& out, const system::chain::transaction& tx,
        const system::chain::context& ctx) NOEXCEPT;
    code unconfirmable(const database::header_link& link, size_t height,
        const code& ec) NOEXCEPT;
    void broadcast(const system::chain::transaction& tx) const NOEXCEPT;
    void start_scan(bool remap) NOEXCEPT;
    void start_revalidation() NOEXCEPT;

    /// Relay the txs with their unconfirmed ancestry, marked held by origin,
    /// to each of the slots whose rate the sum of the rows it lacks clears.
    bool relay(const database::tx_links& links, size_t origin,
        uint64_t slots) NOEXCEPT;

    /// Relay the confirmable txs in row order, each with the ancestry the
    /// slot lacks, where that clears the slot's rate.
    bool sweep_relay(size_t slot) NOEXCEPT;

    /// The marks of the pool row, grown with the table.
    uint64_t& marks(size_t row) NOEXCEPT;

    /// The pool index follows blocks (pooling store and relay enabled).
    bool is_indexing() const NOEXCEPT;

    /// A validation under the context holds at a later block.
    bool reusable(const system::chain::context& ctx) const NOEXCEPT;

    /// A validation under the context holds under the pool context.
    bool is_sufficient(const system::chain::context& ctx) const NOEXCEPT;

    /// Sweep the index by the pool context if due, false on store failure.
    bool sweep() NOEXCEPT;
    static uint64_t arriving(const query::pool_arrivals& arrivals,
        uint64_t rate, estimate_mode mode) NOEXCEPT;

    /// Notify pool subscribers of the scan, timed from its start, and fire
    /// its event (other than for coverage, which fires only when it clears).
    using time_point = std::chrono::steady_clock::time_point;
    static time_point now() NOEXCEPT;
    void report(pool_scan scan, const time_point& start) NOEXCEPT;

    /// Fire the event with the microseconds since the start.
    void fire_span(events event, const time_point& start) NOEXCEPT;

    // These are thread safe.
    network::threadpool scan_threadpool_;
    network::threadpool revalidation_threadpool_;
    stopper stopping_{};
    std::atomic_bool pooling_{};
    std::atomic<size_t> writers_{};
    std::atomic<uint64_t> reserved_{};
    const uint64_t outbound_excluded_;
    const uint64_t inbound_excluded_;

    // These are protected by strand.
    pool_subscriber subscriber_{};
    system::chain::context pool_{};
    size_t floor_{};
    bool due_{ true };
    bool scanning_{};
    bool revalidating_{};
    uint32_t revalidated_flags_{};
    std::vector<uint64_t> marks_{};
    std::array<uint64_t, relay_slots> rates_{};
};

} // namespace node
} // namespace libbitcoin

#endif
