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
#include <bitcoin/node/block_arena.hpp>

#include <algorithm>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;

// construct/destruct/assign
// ----------------------------------------------------------------------------

block_arena::block_arena(size_t multiple) NOEXCEPT
  : memory_map_{ nullptr },
    multiple_{ multiple },
    offset_{ zero },
    total_{ zero },
    size_{ zero }
{
}

block_arena::block_arena(block_arena&& other) NOEXCEPT
  : memory_map_{ other.memory_map_ },
    multiple_{ other.multiple_ },
    offset_{ other.offset_ },
    total_{ other.total_ },
    size_{ other.size_ }
{
    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
}

block_arena::~block_arena() NOEXCEPT
{
}

block_arena& block_arena::operator=(block_arena&& other) NOEXCEPT
{
    memory_map_ = other.memory_map_;
    multiple_ = other.multiple_;
    offset_ = other.offset_;
    total_ = other.total_;
    size_ = other.size_;

    // Prevents free(memory_map_) as responsibility is passed to this object.
    other.memory_map_ = nullptr;
    return *this;
}

// public
// ----------------------------------------------------------------------------

void* block_arena::start(size_t wire_size) THROWS
{
    if (is_multiply_overflow(wire_size, multiple_))
        throw allocation_exception{};

    size_ = wire_size * multiple_;
    memory_map_ = nullptr;
    offset_ = zero;
    total_ = zero;
    push();
    return memory_map_;
}

size_t block_arena::detach() NOEXCEPT
{
    memory_map_ = nullptr;
    return total_ + offset_;
}

void block_arena::release(void* address) NOEXCEPT
{
    while (!is_null(address))
    {
        const auto link = get_link(pointer_cast<uint8_t>(address));
        free_(address);
        address = link;
    }
}

// protected
// ----------------------------------------------------------------------------

void block_arena::push(size_t minimum) THROWS
{
    static constexpr size_t link_size = sizeof(void*);

    // Ensure next allocation accomodates link plus current request.
    BC_ASSERT(!is_add_overflow(minimum, link_size));
    size_ = std::max(size_, minimum + link_size);
    const auto map = pointer_cast<uint8_t>(malloc_(size_));

    if (is_null(map))
        throw allocation_exception{};

    // Set previous chunk's link pointer to the new allocation.
    set_link(map);
    memory_map_ = map;

    // Set current chunk's link pointer to nullptr.
    set_link(nullptr);
    total_ += offset_;
    offset_ = link_size;
}

// protected interface
// ----------------------------------------------------------------------------

void* block_arena::do_allocate(size_t bytes, size_t align) THROWS
{
    BC_ASSERT(!is_null(memory_map_));
    const auto aligned_offset = to_aligned(offset_, align);
    const auto padding = aligned_offset - offset_;

    BC_ASSERT(!system::is_add_overflow(padding, bytes));
    const auto allocation = padding + bytes;

    if (allocation > capacity())
    {
        push(allocation);
        return do_allocate(bytes, align);
    }
    else
    {
        offset_ += allocation;

        BC_PUSH_WARNING(NO_POINTER_ARITHMETIC)
        return memory_map_ + aligned_offset;
        BC_POP_WARNING()
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

} // namespace node
} // namespace libbitcoin
