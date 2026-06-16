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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP

#include <atomic>
#include <mutex>
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
    /// Post a method in base or derived class in parallel (use PARALLEL).
    template <class Derived, typename Method, typename... Args>
    inline auto parallel(Method&& method, Args&&... args) NOEXCEPT
    {
        return boost::asio::post(validation_threadpool_.service(),
            BIND_TO(method, args));
    }

    typedef network::race_unity<const code&, const database::tx_link&> race;

    virtual bool handle_chase(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_advanced(height_t height) NOEXCEPT;
    virtual void do_checked(height_t height) NOEXCEPT;
    virtual void do_bumped(height_t height) NOEXCEPT;
    virtual void do_bump(height_t height) NOEXCEPT;

    virtual void post_block(const database::header_link& link,
        bool bypass) NOEXCEPT;
    virtual void validate_block(const database::header_link& link,
        bool bypass) NOEXCEPT;
    virtual code validate(bool& batched, bool& faulted, bool bypass,
        const system::chain::block& block, const database::header_link& link,
        const system::chain::context& ctx) NOEXCEPT;
    virtual code populate(bool bypass, const system::chain::block& block,
        const system::chain::context& ctx) NOEXCEPT;
    virtual void complete_block(const code& ec,
        const database::header_link& link, size_t height, bool bypass,
        bool batched=false, bool faulted=false) NOEXCEPT;
    virtual void notify_block(const code& ec, size_t height,
        const database::header_link& link, bool bypass) NOEXCEPT;
    virtual void push_batch(const database::header_link& link) NOEXCEPT;
    virtual void process_batch() NOEXCEPT;

    // Override base class strand because it sits on the network thread pool.
    network::asio::strand& strand() NOEXCEPT override;
    bool stranded() const NOEXCEPT override;

private:
    static constexpr auto relaxed = std::memory_order_relaxed;
    using shared_lock = const std::shared_lock<std::shared_mutex>;
    using shared_lock_cptr = std::shared_ptr<shared_lock>;
    using atomic_counter = std::atomic<size_t>;
    using atomic_counter_ptr = std::shared_ptr<atomic_counter>;
    using signatures = system::chain::signatures;
    using threshold_group = signatures::threshold_group;
    using missed = signatures::miss;

    signatures get_capture(const database::header_link& link) NOEXCEPT;

    // Handlers.
    void do_log(const system::chain::script& missed) NOEXCEPT;
    void do_fire(missed miss, size_t count,
        const shared_lock_cptr& lock) NOEXCEPT;
    bool do_ecdsa(const system::hash_digest& digest,
        const system::ec_compressed& point, const system::ec_signature& sign,
        const database::header_link& link) NOEXCEPT;
    bool do_schnorr(const system::hash_digest& digest,
        const system::ec_xonly& point, const system::ec_signature& sign,
        const database::header_link& link) NOEXCEPT;
    bool do_multisig(const system::hash_digest& digest,
        const system::ec_compresseds& points,
        const system::ec_signatures& signs, const database::header_link& link,
        const atomic_counter_ptr& sequence) NOEXCEPT;
    bool do_threshold(const threshold_group& group,
        const database::header_link& link,
        const atomic_counter_ptr& sequence) NOEXCEPT;

    void log_capture(const std::string_view& name,
        size_t captured, size_t missed) const NOEXCEPT;
    void log_captures() const NOEXCEPT;

    // These are protected by strand.
    database::header_links batched_{};
    network::threadpool validation_threadpool_;

    // These are thread safe.

    // This prevents table updates during batch verify.
    std::shared_mutex mutex_{};

    std::atomic<size_t> ecdsa_{};
    std::atomic<size_t> schnorr_{};
    std::atomic<size_t> multisig_{};
    std::atomic<size_t> threshold_{};

    std::atomic<size_t> missed_ecdsa_{};
    std::atomic<size_t> missed_schnorr_{};
    std::atomic<size_t> missed_multisig_{};
    std::atomic<size_t> missed_threshold_{};

    std::atomic<size_t> backlog_{};
    network::asio::strand validation_strand_;
    const uint32_t subsidy_interval_;
    const uint64_t initial_subsidy_;
    const size_t maximum_backlog_;
    const bool batch_signatures_;
    const bool node_witness_;
    const bool filter_;
};

} // namespace node
} // namespace libbitcoin

#endif
