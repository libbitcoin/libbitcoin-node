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
#include <bitcoin/node/block_arena.hpp>

#include <shared_mutex>
#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {

// TODO: initialize memory.
block_arena::block_arena() NOEXCEPT
{
}

// TODO: block on mutex until exclusive and then free memory.
block_arena::~block_arena() NOEXCEPT
{
}

// TODO: if aligned size is insufficient block on mutex until memory remapped.
void* block_arena::do_allocate(size_t bytes, size_t) THROWS
{
    BC_PUSH_WARNING(NO_NEW_OR_DELETE)
    return ::operator new(bytes);
    BC_POP_WARNING()
}

// TODO: change to nop.
void block_arena::do_deallocate(void* ptr, size_t, size_t) NOEXCEPT
{
    BC_PUSH_WARNING(NO_NEW_OR_DELETE)
    ::operator delete(ptr);
    BC_POP_WARNING()
}

bool block_arena::do_is_equal(const arena& other) const NOEXCEPT
{
    // Do not cross the streams.
    return &other == this;
}

} // namespace node
} // namespace libbitcoin
