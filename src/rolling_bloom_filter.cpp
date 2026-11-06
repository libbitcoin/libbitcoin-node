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
#include <bitcoin/node/rolling_bloom_filter.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void rolling_bloom_filter::insert(const hash_digest& hash) NOEXCEPT
{
    if (count_ == generation)
    {
        current_ = add1(current_) % generations;
        known_[current_].reset();
        count_ = zero;
    }

    auto& known = known_[current_];
    if (!known)
        known = emplace_shared<filter::type>();

    filter::screen(*known, entropy(hash));
    ++count_;
}

bool rolling_bloom_filter::contains(const hash_digest& hash) const NOEXCEPT
{
    const auto value = entropy(hash);
    const auto screened = [&](const table& known) NOEXCEPT
    {
        return known && filter::is_screened(*known, value);
    };

    return std::any_of(known_.cbegin(), known_.cend(), screened);
}

// private
uint64_t rolling_bloom_filter::entropy(const hash_digest& hash) NOEXCEPT
{
    return unsafe_from_little_endian<uint64_t>(hash.data());
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
