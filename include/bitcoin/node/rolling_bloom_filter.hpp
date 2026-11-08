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
#ifndef LIBBITCOIN_NODE_ROLLING_BLOOM_FILTER_HPP
#define LIBBITCOIN_NODE_ROLLING_BLOOM_FILTER_HPP

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Not thread safe rolling bloom filter of hashes, by generation.
class BCN_API rolling_bloom_filter
{
public:
    /// Insert the hash, replacing the oldest generation when current is full.
    void insert(const system::hash_digest& hash) NOEXCEPT;

    /// The hash is (probably) inserted.
    bool contains(const system::hash_digest& hash) const NOEXCEPT;

private:
    static uint64_t entropy(const system::hash_digest& hash) NOEXCEPT;

    // Retains at least the most recent (generations - 1) * generation.
    static constexpr auto generations = 3_size;
    static constexpr auto generation = network::messages::peer::max_inventory;

    // False positive rate of about 2^-probes per generation.
    static constexpr auto probes = 20_size;

    // Optimal bits per generation, generation * probes / ln(2).
    static constexpr auto micro = 1'000'000_u64;
    static constexpr auto ln2_micro = 693'147_u64;
    static constexpr auto load = system::multiply<uint64_t>(generation, probes);
    static constexpr auto scaled = system::multiply(load, micro);
    static constexpr auto rounded = system::ceilinged_divide(scaled, ln2_micro);
    static constexpr auto optimal = system::possible_narrow_cast<size_t>(rounded);

    using filter = system::bloom_array<optimal, probes>;
    using table = std::shared_ptr<filter::type>;

    // These are not thread safe.
    std::array<table, generations> known_{};
    size_t current_{};
    size_t count_{};
};

} // namespace node
} // namespace libbitcoin

#endif
