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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP

#include <atomic>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks in the the candidate header chain for validation.
class BCN_API chaser_validate
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_validate);

    chaser_validate(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;
    void stop() NOEXCEPT override;

protected:
    using header_link = database::header_link;
    using header_links = database::header_links;
    using signatures = system::chain::signatures;
    using race = network::race_unity<const code&, const database::tx_link&>;

    /// Post a method in base or derived class in parallel (use PARALLEL).
    template <class Derived, typename Method, typename... Args>
    inline auto parallel(Method&& method, Args&&... args) NOEXCEPT
    {
        return boost::asio::post(validation_threadpool_.service(),
            BIND_TO(method, args));
    }

    virtual bool handle_chase(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_checked(height_t height) NOEXCEPT;
    virtual void do_bumped(height_t height) NOEXCEPT;
    virtual void do_bump(height_t height) NOEXCEPT;

    /// Validation.
    virtual void post_block(const header_link& link, bool bypass) NOEXCEPT;
    virtual void validate_block(const header_link& link, bool bypass) NOEXCEPT;
    virtual code validate(bool& batched, bool& capturing, bool bypass,
        const system::chain::block& block, const header_link& link,
        const system::chain::context& ctx) NOEXCEPT;
    virtual code populate(bool bypass, const system::chain::block& block,
        const system::chain::context& ctx) NOEXCEPT;
    virtual void complete_block(const code& ec, const header_link& link,
        size_t height, bool bypass, bool batched=false,
        bool capturing=false) NOEXCEPT;
    virtual void notify_block(const code& ec, size_t height,
        const header_link& link, bool bypass, bool startup=false) NOEXCEPT;

    /// Batching (lock-free, self-serviced by completing pool threads).
    /// Batch state is the store: sig tables carry rows, prevalid table
    /// carries the per-block link set (write-through, crash-durable).
    virtual code start_batch() NOEXCEPT;
    virtual void process_batch(bool residual) NOEXCEPT;
    virtual code do_process_batch(bool startup) NOEXCEPT;
    virtual bool mark_valids(header_links& prevalids, bool startup) NOEXCEPT;
    virtual bool mark_invalids(header_links& prevalids,
        const header_links& invalids, bool startup) NOEXCEPT;

    /// Turnstile (drain/commit exclusion without blocking writers).
    virtual bool enter_capture() NOEXCEPT;
    virtual void exit_capture() NOEXCEPT;

    // Override base class strand because it sits on the network thread pool.
    network::asio::strand& strand() NOEXCEPT override;
    bool stranded() const NOEXCEPT override;

private:
    using atomic_counter = std::atomic<size_t>;
    struct counters
    {
        atomic_counter ecdsa_{};
        atomic_counter schnorr_{};
        atomic_counter multisig_{};
        atomic_counter threshold_{};
        atomic_counter missed_ecdsa_{};
        atomic_counter missed_schnorr_{};
        atomic_counter missed_multisig_{};
        atomic_counter missed_threshold_{};
    };

    using missed = signatures::miss;

    // Capture handlers.
    void do_log(const system::chain::script& missed) NOEXCEPT;
    void do_fire(missed miss, size_t count) NOEXCEPT;

    // Capture helpers.
    signatures get_capture(const header_link& link) NOEXCEPT;
    code commit_capture(bool& batched, const header_link& link) NOEXCEPT;
    void clear_capture() NOEXCEPT;
    void do_purge_capture() NOEXCEPT;
    std::string log_ratio(const std::string& name, size_t numerator,
        size_t denominator) const NOEXCEPT;
    void log_captures() const NOEXCEPT;

    // Batching helpers.
    bool is_residual() NOEXCEPT;
    bool is_mature(bool residual) NOEXCEPT;
    std::string log_rate(const std::string& name, size_t signatures,
        size_t milliseconds) const NOEXCEPT;

    // This is not thread safe.
    network::threadpool validation_threadpool_;

    // These are thread safe.
    network::asio::strand validation_strand_;
    atomic_counter validate_backlog_{};
    std::atomic_bool disk_recovering_{};
    std::atomic_bool window_archived_{};
    std::atomic_bool maximum_posted_{};
    ////std::atomic_bool verifying_{};
    std::atomic_bool draining_{};
    atomic_counter writers_{};
    counters counters_{};
    stopper stopping_{};

    // These are thread safe.
    const uint32_t subsidy_interval_;
    const uint64_t initial_subsidy_;
    const size_t silent_start_height_;
    const size_t maximum_backlog_;
    const size_t maximum_height_;
    const uint64_t batch_target_;
    const bool batch_enabled_;
    const bool node_witness_;
    const bool filter_;
};

} // namespace node
} // namespace libbitcoin

#endif
