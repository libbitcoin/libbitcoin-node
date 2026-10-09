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
#ifndef LIBBITCOIN_NODE_ESTIMATOR_HPP
#define LIBBITCOIN_NODE_ESTIMATOR_HPP

#include <atomic>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Fee estimator with contained accumulator.
/// Accumulator is typically too large for stack creation.
/// Thread safe, blocking calls to estimate() during updates.
/// Initialize on chain current coalesce after snapshot/prune.
/// If chain falls > 1008/2 (?) blocks behind, reset and wait for coalesce.
/// Only txs pooled before confirmation contribute, by blocks waited.
class BCN_API estimator
{
public:
    typedef std::unique_ptr<estimator> ptr;
    static constexpr size_t maximum_horizon = 1008;
    static constexpr size_t estimate_failed = max_uint64;

    DELETE_COPY_MOVE_DESTRUCT(estimator);

    /// Estimation modes.
    enum class mode
    {
        basic,
        geometric,
        economical,
        conservative,
        unknown
    };

    /// Construct (use heap allocation).
    estimator() NOEXCEPT {};

    /// Fee estimation in satoshis/transaction virtual size (not thread safe).
    /// Pass zero to target next block for confirmation, range:0..1007.
    uint64_t estimate(size_t target, mode mode) const NOEXCEPT;

    /// Populate accumulator with count blocks up to the top confirmed block,
    /// and with the unconfirmed pooled transactions.
    bool initialize(const std::atomic_bool& cancel, const query& query,
        size_t count=maximum_horizon) NOEXCEPT;

    /// Update accumulator (not thread safe).
    bool push(const query& query) NOEXCEPT;
    bool pop(const query& query) NOEXCEPT;
    bool pool(const query& query, const database::tx_link& link) NOEXCEPT;

    /// Top height of accumulator (thread safe).
    size_t top_height() const NOEXCEPT;

protected:
    using rate = database::fee_rate;
    using rates = database::fee_rates;
    using rate_sets = database::fee_rate_sets;
    using bins = std::array<size_t, 283>;

    /// Bucket depth sizing parameters (number of blocks).
    enum horizon : size_t
    {
        /// 2 hrs × 60 mins/hr / 10 mins/block = 12 blocks.
        small  = 12,

        /// 8 hrs × 60 mins/hr / 10 mins/block = 48 blocks.
        medium = 48,

        /// 7 days * 24 hrs/day × 60 mins/hr / 10 mins/block = 1008 blocks.
        large  = maximum_horizon
    };

    /// Bucket count sizing parameters.
    struct sizing
    {
        static constexpr double min  = 0.1;
        static constexpr double max  = 100'000.0;
        static constexpr double step = 1.05;

        /// Derived from min/max/step above.
        static constexpr size_t count = std::tuple_size_v<bins>;
    };

    /// Estimation confidences.
    struct confidence
    {
        static constexpr double low = 0.60;
        static constexpr double mid = 0.85;
        static constexpr double high = 0.95;
    };

    /// Accumulator (persistent, decay-weighted counters).
    struct accumulator
    {
        template <size_t Horizon>
        struct bucket
        {
            /// Total scaled txs in bucket.
            size_t total{};

            /// confirmed[n]: scaled txs not confirmed within n+1 blocks.
            std::array<size_t, Horizon> confirmed;
        };

        /// Current block height of accumulated state.
        std::atomic<size_t> top_height{};

        /// Accumulated scaled fee in decayed buckets by horizon.
        /// Array count is the half life of the decay it implies.
        std::array<bucket<horizon::small>,  sizing::count> small{};
        std::array<bucket<horizon::medium>, sizing::count> medium{};
        std::array<bucket<horizon::large>,  sizing::count> large{};

        /// Unconfirmed pooled txs by bin, in slots of pool entry height.
        std::array<bins, horizon::large> pending{};
        std::array<size_t, horizon::large> entries{};

        /// Unconfirmed pooled txs displaced from the pending slots.
        bins aged{};
    };

    // C++23: make consteval.
    static inline double decay_rate() NOEXCEPT
    {
        static const auto rate = std::pow(0.5, 1.0 / sizing::count);
        return rate;
    }

    // C++23: make constexpr.
    static inline double to_scale_term(size_t age) NOEXCEPT
    {
        return system::power(decay_rate(), age);
    }

    // C++23: make constexpr.
    static inline double to_scale_factor(bool push) NOEXCEPT
    {
        return std::pow(decay_rate(), push ? +1.0 : -1.0);
    }

    /// Bin of the tx rate, false if the tx is below the minimum rate.
    static bool to_bin(size_t& out, const rate& tx) NOEXCEPT;

    accumulator& history() NOEXCEPT;
    const accumulator& history() const NOEXCEPT;
    bool initialize(const rate_sets& blocks, const rates& pooled,
        size_t top) NOEXCEPT;
    bool push(const rates& block) NOEXCEPT;
    bool pop(const rates& block) NOEXCEPT;
    bool pool(const rate& tx) NOEXCEPT;
    uint64_t compute(size_t target, double confidence,
        bool geometric=false) const NOEXCEPT;

private:
    using waits = std::array<size_t, add1(maximum_horizon)>;

    bool update(const rates& block, size_t height, bool push) NOEXCEPT;
    void pend(size_t bin, size_t height, bool enter) NOEXCEPT;
    void pending(bins& failed, size_t target) const NOEXCEPT;
    void decay(auto& buckets, double factor) NOEXCEPT;
    void decay(bool push) NOEXCEPT;

    accumulator fees_{};
    std::array<waits, sizing::count> waits_{};
};

} // namespace node
} // namespace libbitcoin

#endif
