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

#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {

block_arena::block_arena() NOEXCEPT
{
}

BC_PUSH_WARNING(NO_NEW_OR_DELETE)

void* block_arena::do_allocate(size_t bytes, size_t) THROWS
{
    return ::operator new(bytes);
}

void block_arena::do_deallocate(void* ptr, size_t, size_t) NOEXCEPT
{
    ::operator delete(ptr);
}

BC_POP_WARNING()

bool block_arena::do_is_equal(const arena& other) const NOEXCEPT
{
    // Do not cross the streams.
    return &other == this;
}

} // namespace node
} // namespace libbitcoin
