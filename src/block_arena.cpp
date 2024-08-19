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
    
BC_DEBUG_ONLY(constexpr auto max_align = alignof(std::max_align_t);)

template <typename Type, if_unsigned_integer<Type> = true>
constexpr Type to_aligned(Type value, Type alignment) NOEXCEPT
{
    return (value + sub1(alignment)) & ~sub1(alignment);
}

namespace node {

BC_PUSH_WARNING(NO_MALLOC_OR_FREE)
BC_PUSH_WARNING(NO_POINTER_ARITHMETIC)
BC_PUSH_WARNING(THROW_FROM_NOEXCEPT)

// "If size is zero, the behavior of malloc is implementation-defined. For
// example, a null pointer may be returned. Alternatively, a non-null pointer
// may be returned; but such a pointer should not be dereferenced, and should
// be passed to free to avoid memory leaks."
// en.cppreference.com/w/c/memory/malloc

block_arena::block_arena(size_t size) NOEXCEPT
  : size_{ size },
    offset_{ size }
{
}

block_arena::block_arena(block_arena&& other) NOEXCEPT
  : memory_map_{ other.memory_map_ },
    size_{ other.size_ },
    offset_{ other.offset_ }
{
    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
}

block_arena::~block_arena() NOEXCEPT
{
    release(memory_map_, offset_);
}

block_arena& block_arena::operator=(block_arena&& other) NOEXCEPT
{
    memory_map_ = other.memory_map_;
    size_ = other.size_;
    offset_ = other.offset_;

    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
    return *this;
}

void* block_arena::start() NOEXCEPT
{
    release(memory_map_, offset_);
    memory_map_ = system::pointer_cast<uint8_t>(std::malloc(size_));
    if (is_null(memory_map_))
        throw allocation_exception{};

    offset_ = zero;
    return memory_map_;
}

size_t block_arena::detach() NOEXCEPT
{
    const auto size = offset_;
    const auto map = std::realloc(memory_map_, size);

    // Memory map must not move.
    if (map != memory_map_)
        throw allocation_exception{};

    memory_map_ = nullptr;
    offset_ = size_;
    return size;
}

void block_arena::release(void* ptr, size_t) NOEXCEPT
{
    // Does not affect member state.
    if (!is_null(ptr))
        std::free(ptr);
}

void* block_arena::do_allocate(size_t bytes, size_t align) THROWS
{
    using namespace system;
    BC_ASSERT_MSG(is_nonzero(align), "align zero");
    BC_ASSERT_MSG(align <= max_align, "align overflow");
    BC_ASSERT_MSG(power2(floored_log2(align)) == align, "align power");
    BC_ASSERT_MSG(!is_add_overflow(bytes, sub1(align)), "alignment overflow");

    const auto aligned_offset = to_aligned(offset_, align);
    const auto padding = aligned_offset - offset_;
    const auto allocation = padding + bytes;

    ////BC_ASSERT_MSG(allocation <= capacity(), "buffer overflow");
    if (allocation > capacity())
        throw allocation_exception{};

    offset_ += allocation;
    return memory_map_ + aligned_offset;
}

void block_arena::do_deallocate(void*, size_t, size_t) NOEXCEPT
{
}

bool block_arena::do_is_equal(const arena& other) const NOEXCEPT
{
    // Do not cross the streams.
    return &other == this;
}

// private
size_t block_arena::capacity() const NOEXCEPT
{
    return system::floored_subtract(size_, offset_);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
