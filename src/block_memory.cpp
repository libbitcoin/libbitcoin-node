/**
 * Copyright (c) 2011-2024 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/block_memory.hpp>

#include <atomic>
#include <memory>
#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

block_memory::block_memory(size_t bytes, size_t threads) NOEXCEPT
  : count_{}, arenas_{}
{
    arenas_.reserve(threads);
    for (auto index = zero; index < threads; ++index)
        arenas_.emplace_back(bytes);
}

arena* block_memory::get_arena() NOEXCEPT
{
    return get_block_arena();
}

retainer::ptr block_memory::get_retainer(size_t allocation) NOEXCEPT
{
    return std::make_shared<retainer>(get_block_arena()->get_mutex(),
        allocation);
}

// protected
block_arena* block_memory::get_block_arena() const THROWS
{
    static thread_local auto index = count_.fetch_add(one,
        std::memory_order_relaxed);

    // More threads are requesting an arena than specified at construct.
    ////BC_ASSERT(index < arenas_.size());
    if (index >= arenas_.size())
        throw allocation_exception{};

    return &arenas_.at(index);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
