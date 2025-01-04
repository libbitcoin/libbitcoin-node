/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_BLOCK_ARENA_HPP
#define LIBBITCOIN_NODE_BLOCK_ARENA_HPP

#include <shared_mutex>
#include <bitcoin/system.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Thread UNSAFE detachable linked-linear memory arena.
class BCN_API block_arena
  : public arena
{
public:
    DELETE_COPY(block_arena);
    
    block_arena(size_t multiple) NOEXCEPT;
    block_arena(block_arena&& other) NOEXCEPT;
    virtual ~block_arena() NOEXCEPT;

    block_arena& operator=(block_arena&& other) NOEXCEPT;

    /// Start an allocation of linked chunks.
    NODISCARD void* start(size_t wire_size) THROWS override;

    /// Finalize allocation and reset allocator, return total allocation.
    size_t detach() NOEXCEPT override;

    /// Release all chunks chained to the address.
    void release(void* address) NOEXCEPT override;

protected:
    /// Determine alignment offset.
    static constexpr size_t to_aligned(size_t value, size_t align) NOEXCEPT
    {
        using namespace system;
        BC_ASSERT_MSG(is_nonzero(align), "align zero");
        BC_ASSERT_MSG(!is_add_overflow(value, sub1(align)), "overflow");
        BC_ASSERT_MSG(power2(floored_log2(align)) == align, "align power");
        BC_ASSERT_MSG(align <= alignof(std::max_align_t), "align overflow");
        return (value + sub1(align)) & ~sub1(align);
    }

    /// Malloc throws if memory is not allocated.
    virtual INLINE void* malloc(size_t bytes) THROWS
    {
        BC_PUSH_WARNING(NO_MALLOC_OR_FREE)
        return std::malloc(bytes);
        BC_POP_WARNING()
    }

    /// Free does not throw, behavior is undefined if address is incorrect.
    virtual INLINE void free(void* address) NOEXCEPT
    {
        BC_PUSH_WARNING(NO_MALLOC_OR_FREE)
        std::free(address);
        BC_POP_WARNING()
    }

    /// Link a memory chunk to the allocated stack.
    void push(size_t minimum=zero) THROWS;

    /// Close out chunk with link to next.
    INLINE void set_link(uint8_t* next_address) NOEXCEPT
    {
        // Don't set previous when current is the first chunk.
        if (!is_null(memory_map_))
        {
            BC_PUSH_WARNING(NO_REINTERPRET_CAST)
            reinterpret_cast<void*&>(*memory_map_) = next_address;
            BC_POP_WARNING()
        }
    }

    /// Get size of address chunk and address of next chunk (or nullptr).
    INLINE void* get_link(uint8_t* address) const NOEXCEPT
    {
        BC_ASSERT(!is_null(address));
        BC_PUSH_WARNING(NO_REINTERPRET_CAST)
        return reinterpret_cast<void*&>(*address);
        BC_POP_WARNING()
    }

    /// Number of bytes remaining to be allocated.
    INLINE size_t capacity() const NOEXCEPT
    {
        return system::floored_subtract(size_, offset_);
    }

    void* do_allocate(size_t bytes, size_t align) THROWS override;
    void do_deallocate(void* ptr, size_t bytes, size_t align) NOEXCEPT override;
    bool do_is_equal(const arena& other) const NOEXCEPT override;

    // These are unprotected, caller must guard.
    uint8_t* memory_map_;
    size_t multiple_;
    size_t offset_;
    size_t total_;
    size_t size_;
};

} // namespace node
} // namespace libbitcoin

#endif
