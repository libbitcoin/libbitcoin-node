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

#include <algorithm>
#include <shared_mutex>
#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {

BC_PUSH_WARNING(NO_MALLOC_OR_FREE)
BC_PUSH_WARNING(NO_REINTERPRET_CAST)
BC_PUSH_WARNING(NO_POINTER_ARITHMETIC)

// construct/destruct/assign
// ----------------------------------------------------------------------------

block_arena::block_arena(size_t size) NOEXCEPT
  : memory_map_{ nullptr },
    size_{ std::max(size, record_size) },
    offset_{ size_ },
    total_{ zero }
{
}

block_arena::block_arena(block_arena&& other) NOEXCEPT
  : memory_map_{ other.memory_map_ },
    size_{ other.size_ },
    offset_{ other.offset_ },
    total_{ other.total_ }
{
    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
}

block_arena::~block_arena() NOEXCEPT
{
    release(memory_map_);
}

block_arena& block_arena::operator=(block_arena&& other) NOEXCEPT
{
    memory_map_ = other.memory_map_;
    size_ = other.size_;
    offset_ = other.offset_;
    total_ = other.total_;

    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
    return *this;
}

// public
// ----------------------------------------------------------------------------

void* block_arena::start() THROWS
{
    release(memory_map_);
    reset();
    return attach(zero);
}

size_t block_arena::detach() THROWS
{
    trim_to_offset();
    set_record(nullptr, offset_);
    return reset();
}

void block_arena::release(void* address) NOEXCEPT
{
    while (!is_null(address))
    {
        const auto value = get_record(system::pointer_cast<uint8_t>(address));
        std::free(address/*, value.size */);
        address = value.next;
    } 
}

// protected
// ----------------------------------------------------------------------------

void* block_arena::attach(size_t minimum) THROWS
{
    using namespace system;

    // Ensure next allocation accomodates record plus request (expandable).
    BC_ASSERT(!is_add_overflow(minimum, record_size));
    size_ = std::max(size_, minimum + record_size);

    // Allocate size to temporary.
    const auto map = pointer_cast<uint8_t>(std::malloc(size_));
    if (is_null(map))
        throw allocation_exception{};

    // Set previous chunk record pointer to new allocation and own size.
    set_record(map, offset_);
    offset_ = record_size;
    return memory_map_ = map;
}

void block_arena::trim_to_offset() THROWS
{
    // Memory map must not move. Move by realloc is allowed but not expected
    // for truncation. If moves then this should drop into mmap/munmap/mremap.
    const auto map = std::realloc(memory_map_, offset_);
    if (map != memory_map_)
        throw allocation_exception{};
}

void block_arena::set_record(uint8_t* next_address, size_t own_size) NOEXCEPT
{
    // Don't set previous when current is the first chunk.
    if (is_null(memory_map_))
        return;

    reinterpret_cast<record&>(*memory_map_) = { next_address, own_size };
    total_ += own_size;
}

block_arena::record block_arena::get_record(uint8_t* address) const NOEXCEPT
{
    return reinterpret_cast<const record&>(*address);
}

size_t block_arena::capacity() const NOEXCEPT
{
    return system::floored_subtract(size_, offset_);
}

size_t block_arena::reset() NOEXCEPT
{
    const auto total = total_;
    memory_map_ = nullptr;
    offset_ = size_;
    total_ = zero;
    return total;
}

// protected interface
// ----------------------------------------------------------------------------

void* block_arena::do_allocate(size_t bytes, size_t align) THROWS
{
    const auto aligned_offset = to_aligned(offset_, align);
    const auto padding = aligned_offset - offset_;
    const auto allocation = padding + bytes;
    BC_ASSERT(!system::is_add_overflow(padding, bytes));

    if (allocation > capacity())
    {
        trim_to_offset();
        attach(allocation);
        return do_allocate(bytes, align);
    }
    else
    {
        offset_ += allocation;
        return memory_map_ + aligned_offset;
    }
}

void block_arena::do_deallocate(void*, size_t, size_t) NOEXCEPT
{
}

bool block_arena::do_is_equal(const arena& other) const NOEXCEPT
{
    // Do not cross the streams.
    return &other == this;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
