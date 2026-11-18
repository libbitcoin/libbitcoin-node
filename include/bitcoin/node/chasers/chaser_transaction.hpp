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

#include <chrono>
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
        const system::chain::transactions_cptr& txs,
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

    /// Subscribe to pool scans (notified on the strand).
    virtual void subscribe_pool(pool_notifier&& handler) NOEXCEPT;

protected:
    using chaser::broadcast;

    virtual bool handle_chase(const code& ec, event_value value) NOEXCEPT;

    virtual void do_submit(const system::chain::transactions_cptr& txs,
        bool test, bool announced, const submit_handler& handler) NOEXCEPT;
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

    /// Recompute the pool context and sweep the index by it if due, closing
    /// the pool if not current or the context is not reusable, and map scan
    /// starts: built once current, extended by block until pooling, and
    /// remapped when a block archived before the latch is organized.
    virtual void do_bump() NOEXCEPT;

    /// Retain the pool subscription.
    virtual void do_subscribe_pool(const pool_notifier& handler) NOEXCEPT;

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

private:
    static size_t to_rate(const system::chain::transaction& tx) NOEXCEPT;
    system::pool::algorithm::totals_t block_limit() const NOEXCEPT;

    code check(const system::chain::transaction_cptrs& txs,
        const std::vector<bool>& stored, bool announced) NOEXCEPT;
    code accept(const system::chain::transaction& tx) NOEXCEPT;
    code admit(const system::chain::transaction& tx, uint32_t time) NOEXCEPT;
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
    stopper stopping_{};
    std::atomic_bool pooling_{};
    std::atomic<size_t> writers_{};

    // These are protected by strand.
    pool_subscriber subscriber_{};
    system::chain::context pool_{};
    size_t floor_{};
    bool due_{ true };
    bool scanning_{};
};

} // namespace node
} // namespace libbitcoin

#endif
