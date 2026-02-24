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
#include <bitcoin/node/block_memory.hpp>

#include <atomic>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

block_memory::block_memory(size_t multiple, size_t threads) NOEXCEPT
{
    if (is_nonzero(multiple))
    {
        arenas_.reserve(threads);
        for (auto index = zero; index < threads; ++index)
            arenas_.emplace_back(multiple);
    }
}

arena* block_memory::get_arena() NOEXCEPT
{
    thread_local auto thread = count_.fetch_add(one, std::memory_order_relaxed);
    return thread < arenas_.size() ? &arenas_.at(thread) : default_arena::get();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
