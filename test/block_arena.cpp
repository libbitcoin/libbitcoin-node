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
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(block_arena_tests)

using namespace system;

constexpr auto link_size = sizeof(void*);

class accessor
  : public block_arena
{
public:
    // public

    using block_arena::block_arena;

    const uint8_t* get_memory_map() const NOEXCEPT
    {
        return memory_map_;
    }

    void set_memory_map(uint8_t* map) NOEXCEPT
    {
        memory_map_ = map;
    }

    size_t get_multiple() const NOEXCEPT
    {
        return multiple_;
    }

    void set_multiple(size_t multiple) NOEXCEPT
    {
        multiple_ = multiple;
    }

    size_t get_offset() const NOEXCEPT
    {
        return offset_;
    }

    void set_offset(size_t offset) NOEXCEPT
    {
        offset_ = offset;
    }

    size_t get_total() const NOEXCEPT
    {
        return total_;
    }

    void set_total(size_t total) NOEXCEPT
    {
        total_ = total;
    }

    size_t get_size() const NOEXCEPT
    {
        return size_;
    }

    void set_size(size_t size) NOEXCEPT
    {
        size_ = size;
    }

    void release(void* address) NOEXCEPT override
    {
        block_arena::release(address);
    }

    // protected

    static constexpr size_t to_aligned_(size_t value, size_t align) NOEXCEPT
    {
        return to_aligned(value, align);
    }

    void* malloc(size_t bytes) THROWS override
    {
        stack.emplace_back(bytes, 0xff_u8);
        return stack.back().data();
    }

    void free(void* address) NOEXCEPT override
    {
        freed.push_back(address);
    }

    void push_(size_t minimum=zero) THROWS
    {
        pushed_minimum = minimum;
        push(minimum);
    }

    void set_link_(uint8_t* next_address) NOEXCEPT
    {
        set_link(next_address);
    }

    void* get_link_(uint8_t* address) const NOEXCEPT
    {
        return get_link(address);
    }

    size_t capacity_() const NOEXCEPT
    {
        return capacity();
    }

    void do_deallocate(void* ptr, size_t size, size_t offset) NOEXCEPT override
    {
        deallocated_ptr = ptr;
        deallocated_size = size;
        deallocated_offset = offset;
        block_arena::do_deallocate(ptr, size, offset);
    }

    // These can be directly invoked via public methods.
    //void* do_allocate(size_t bytes, size_t align) THROWS override;
    //void do_deallocate(void* ptr, size_t bytes, size_t align) NOEXCEPT override;
    //bool do_is_equal(const arena& other) const NOEXCEPT override;

    data_stack stack{};
    std::vector<void*> freed{};
    size_t pushed_minimum{};
    void* deallocated_ptr{};
    size_t deallocated_size{};
    size_t deallocated_offset{};
};

class accessor_null_malloc
    : public accessor
{
public:
    using accessor::accessor;

    void* malloc(size_t) THROWS override
    {
        return nullptr;
    }
};

// construct

BOOST_AUTO_TEST_CASE(block_arena__construct__zero__sets_zero)
{
    constexpr auto multiple = zero;
    const accessor instance{ multiple };
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
    BOOST_REQUIRE_EQUAL(instance.get_multiple(), multiple);
    BOOST_REQUIRE_EQUAL(instance.get_offset(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_total(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);
}

BOOST_AUTO_TEST_CASE(block_arena__construct__value__sets_multiple)
{
    constexpr auto multiple = 42u;
    const accessor instance{ multiple };
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
    BOOST_REQUIRE_EQUAL(instance.get_multiple(), multiple);
    BOOST_REQUIRE_EQUAL(instance.get_offset(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_total(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);
}

BOOST_AUTO_TEST_CASE(block_arena__move_construct__always__nulls_memory_map)
{
    constexpr auto multiple = 42u;
    accessor instance{ multiple };
    system::data_chunk value{ 0x00 };
    auto address = value.data();
    instance.set_memory_map(address);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), address);

    const accessor copy{ std::move(instance) };
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
    BOOST_REQUIRE_EQUAL(copy.get_memory_map(), address);
    BOOST_REQUIRE_EQUAL(copy.get_multiple(), multiple);
    BOOST_REQUIRE_EQUAL(copy.get_offset(), zero);
    BOOST_REQUIRE_EQUAL(copy.get_total(), zero);
    BOOST_REQUIRE_EQUAL(copy.get_size(), zero);
}

// assign

BOOST_AUTO_TEST_CASE(block_arena__assign__always__nulls_memory_map)
{
    constexpr auto multiple = 42u;
    accessor instance{ multiple };
    system::data_chunk value{ 0x00 };
    auto address = value.data();
    instance.set_memory_map(address);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), address);

    const accessor copy = std::move(instance);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
    BOOST_REQUIRE_EQUAL(copy.get_memory_map(), address);
    BOOST_REQUIRE_EQUAL(copy.get_multiple(), multiple);
    BOOST_REQUIRE_EQUAL(copy.get_offset(), zero);
    BOOST_REQUIRE_EQUAL(copy.get_total(), zero);
    BOOST_REQUIRE_EQUAL(copy.get_size(), zero);
}

// start

BOOST_AUTO_TEST_CASE(block_arena__start__multiple_overflow__throws_allocation_exception)
{
    accessor instance{ two };

    BC_PUSH_WARNING(DISCARDING_NON_DISCARDABLE)
    BOOST_REQUIRE_THROW(instance.start(max_size_t), allocation_exception);
    BC_POP_WARNING()
}

BOOST_AUTO_TEST_CASE(block_arena__start__zero__link_size_allocation)
{
    constexpr auto size = zero;
    constexpr auto multiple = 42u;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    BOOST_REQUIRE_EQUAL(instance.stack.front().data(), memory);
    BOOST_REQUIRE_EQUAL(instance.stack.front().size(), link_size);
    BOOST_REQUIRE_EQUAL(instance.get_multiple(), multiple);

    // Push minimum only ensures block allocation sufficient for current do_allocate.
    // The explicit start call allocation is zero minimum because there is no do_allocate.
    BOOST_REQUIRE_EQUAL(instance.pushed_minimum, zero);
}

BOOST_AUTO_TEST_CASE(block_arena__start__at_least_link_size__expected_allocation)
{
    constexpr auto size = 42u;
    constexpr auto multiple = 10u;
    static_assert(size >= link_size);

    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    BOOST_REQUIRE_EQUAL(instance.stack.front().data(), memory);
    BOOST_REQUIRE_EQUAL(instance.stack.front().size(), size * multiple);
    BOOST_REQUIRE_EQUAL(instance.get_multiple(), multiple);
    BOOST_REQUIRE_EQUAL(instance.pushed_minimum, zero);
}

BOOST_AUTO_TEST_CASE(block_arena__start__at_least_link_size__expected_sizes)
{
    constexpr auto size = 42u;
    constexpr auto multiple = 10u;
    static_assert(size >= link_size);

    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    BOOST_REQUIRE_EQUAL(instance.pushed_minimum, zero);

    const auto& chunk = instance.stack.front();
    BOOST_REQUIRE_EQUAL(chunk.data(), memory);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), chunk.data());
    BOOST_REQUIRE_EQUAL(instance.get_size(), size * multiple);
    BOOST_REQUIRE_EQUAL(instance.get_offset(), link_size);

    // Total is total bytes consumed by do_allocate between start and detach, and
    // is used only to indicate the necessary allocation required for the object.
    // Actual allocation will generally exceed total due to chunk granularity.
    // Total not updated until allocated chunk closed out by next push or detach.
    BOOST_REQUIRE_EQUAL(instance.get_total(), zero);
}

BOOST_AUTO_TEST_CASE(block_arena__start__always__sets_nullptr_link)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    static_assert(size * multiple >= link_size);

    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    const auto& chunk = instance.stack.front();
    BOOST_REQUIRE_EQUAL(chunk.data(), memory);
    BOOST_REQUIRE_EQUAL(chunk.size(), size * multiple);

    // malloc is not required to zeroize, the data_chunk mock fills 0xff.
    const data_chunk foo(link_size, 0x00_u8);
    const data_chunk bar(size * multiple - link_size, 0xff_u8);
    const auto buffer = splice(foo, bar);
    BOOST_REQUIRE_EQUAL(chunk, buffer);
}

// detach

BOOST_AUTO_TEST_CASE(block_arena__detach__unstarted__zero_nullptr)
{
    accessor instance{ 10 };
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
    BOOST_REQUIRE_EQUAL(instance.detach(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
}

BOOST_AUTO_TEST_CASE(block_arena__detach__started__link_size_nullptr)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    static_assert(size * multiple >= link_size);

    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), memory);

    // The only used portion of the allocation is the link.
    BOOST_REQUIRE_EQUAL(instance.detach(), link_size);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
}

BOOST_AUTO_TEST_CASE(block_arena__detach__unaligned_allocations__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    constexpr auto first = 3u;
    constexpr auto second = 4u;
    static_assert(size * multiple >= link_size + first + second);

    accessor instance{ multiple };
    const auto memory = pointer_cast<uint8_t>(instance.start(size));
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), memory);
    BOOST_REQUIRE_EQUAL(pointer_cast<uint8_t>(instance.allocate(first, one)), std::next(memory, link_size));
    BOOST_REQUIRE_EQUAL(pointer_cast<uint8_t>(instance.allocate(second, one)), std::next(memory, link_size + first));
    BOOST_REQUIRE_EQUAL(instance.detach(), link_size + first + second);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
}

BOOST_AUTO_TEST_CASE(block_arena__detach__multiple_blocks__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    constexpr auto overflow = multiple * size - link_size;
    static_assert(size * multiple >= link_size + overflow);
    constexpr auto more = 5u;

    accessor instance{ multiple };
    const auto memory = pointer_cast<uint8_t>(instance.start(size));
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), memory);

    // 18 - 8 - 10 = 0 (exact fit)
    BOOST_REQUIRE_EQUAL(pointer_cast<uint8_t>(instance.allocate(overflow, 1)), std::next(memory, link_size));

    // Overflowed to new block, so does not extend opening block.
    const auto used = pointer_cast<uint8_t>(instance.allocate(more, 1));
    BOOST_REQUIRE_NE(used, std::next(memory, link_size + overflow));

    // Extends current (new) block.
    const auto block = instance.get_memory_map();
    BOOST_REQUIRE_EQUAL(used, std::next(block, link_size));

    // Total size is a link for each block and the 15 unaligned bytes allocated.
    BOOST_REQUIRE_EQUAL(instance.detach(), two * link_size + overflow + more);
    BOOST_REQUIRE_EQUAL(instance.get_memory_map(), nullptr);
}

// release

BOOST_AUTO_TEST_CASE(block_arena__release__nullptr__does_not_throw)
{
    accessor instance{ 10 };
    BOOST_REQUIRE_NO_THROW(instance.release(nullptr));
}

BOOST_AUTO_TEST_CASE(block_arena__release__single_block_undetached__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_NO_THROW(instance.release(memory));
    BOOST_REQUIRE_EQUAL(instance.freed.size(), one);
    BOOST_REQUIRE_EQUAL(instance.freed.front(), memory);
}

BOOST_AUTO_TEST_CASE(block_arena__release__single_block_detached__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.detach(), link_size);
    BOOST_REQUIRE_NO_THROW(instance.release(memory));
    BOOST_REQUIRE_EQUAL(instance.freed.size(), one);
    BOOST_REQUIRE_EQUAL(instance.freed.front(), memory);
}

BOOST_AUTO_TEST_CASE(block_arena__release__three_blocks_detached__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    constexpr auto overflow = multiple * size - link_size;
    constexpr auto first = 5u;

    accessor instance{ multiple };
    const auto memory = pointer_cast<uint8_t>(instance.start(size));

    // Does not reallocate.
    const auto position0 = pointer_cast<uint8_t>(instance.allocate(first, 1));
    const auto memory0 = instance.get_memory_map();
    BOOST_REQUIRE_EQUAL(memory0, memory);
    BOOST_REQUIRE_EQUAL(position0, std::next(memory0, link_size));

    // Reallocates.
    const auto position1 = pointer_cast<uint8_t>(instance.allocate(add1(overflow), 1));
    const auto memory1 = instance.get_memory_map();
    BOOST_REQUIRE_NE(memory1, memory0);
    BOOST_REQUIRE_EQUAL(position1, std::next(memory1, link_size));

    // Reallocates.
    const auto position2 = pointer_cast<uint8_t>(instance.allocate(add1(overflow), 1));
    const auto memory2 = instance.get_memory_map();
    BOOST_REQUIRE_NE(memory2, memory1);
    BOOST_REQUIRE_EQUAL(position2, std::next(memory2, link_size));

    BOOST_REQUIRE_EQUAL(instance.detach(), 3u * link_size + first + 2u * add1(overflow));
    BOOST_REQUIRE_NO_THROW(instance.release(memory));
    BOOST_REQUIRE_EQUAL(instance.freed.size(), 3u);
    BOOST_REQUIRE_EQUAL(instance.freed.at(0), memory0);
    BOOST_REQUIRE_EQUAL(instance.freed.at(1), memory1);
    BOOST_REQUIRE_EQUAL(instance.freed.at(2), memory2);
}

// to_aligned

BOOST_AUTO_TEST_CASE(block_arena__to_aligned__ones__expected)
{
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(0, 1), 0u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(1, 1), 1u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(2, 1), 2u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(3, 1), 3u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(4, 1), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(5, 1), 5u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(6, 1), 6u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(7, 1), 7u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(8, 1), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(9, 1), 9u);
}

BOOST_AUTO_TEST_CASE(block_arena__to_aligned__twos__expected)
{
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(0, 2), 0u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(1, 2), 2u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(2, 2), 2u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(3, 2), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(4, 2), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(5, 2), 6u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(6, 2), 6u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(7, 2), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(8, 2), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(10, 2), 10u);
}

BOOST_AUTO_TEST_CASE(block_arena__to_aligned__fours__expected)
{
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(0, 4), 0u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(1, 4), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(2, 4), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(3, 4), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(4, 4), 4u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(5, 4), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(6, 4), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(7, 4), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(8, 4), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(12, 4), 12u);
}

BOOST_AUTO_TEST_CASE(block_arena__to_aligned__eights__expected)
{
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(0, 8), 0u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(1, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(2, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(3, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(4, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(5, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(6, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(7, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(8, 8), 8u);
    BOOST_REQUIRE_EQUAL(accessor::to_aligned_(16, 8), 16u);
}

// push

BOOST_AUTO_TEST_CASE(block_arena__push__null_malloc__throws_allocation_exception)
{
    accessor_null_malloc instance{ 10 };

    BC_PUSH_WARNING(DISCARDING_NON_DISCARDABLE)
    BOOST_REQUIRE_THROW(instance.push_(42), allocation_exception);
    BC_POP_WARNING()
}

BOOST_AUTO_TEST_CASE(block_arena__push__zero_size__sets_minimum_plus_link)
{
    constexpr auto minimum = 7u;
    constexpr auto expected = minimum + link_size;

    accessor instance{ 42 };
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);

    instance.push_(minimum);
    BOOST_REQUIRE_EQUAL(instance.get_size(), expected);
}

BOOST_AUTO_TEST_CASE(block_arena__push__size_minimum_plus_link__unchanged)
{
    constexpr auto minimum = 7u;
    constexpr auto expected = minimum + link_size;

    accessor instance{ 42 };
    instance.set_size(expected);
    instance.push_(minimum);
    BOOST_REQUIRE_EQUAL(instance.get_size(), expected);
}

BOOST_AUTO_TEST_CASE(block_arena__push__size_above_minimum_plus_link__unchanged)
{
    constexpr auto minimum = 7u;
    constexpr auto expected = add1(minimum + link_size);

    accessor instance{ 42 };
    instance.set_size(expected);
    instance.push_(minimum);
    BOOST_REQUIRE_EQUAL(instance.get_size(), expected);
}

// set_link/get_link

BOOST_AUTO_TEST_CASE(block_arena__set_link__nullptr__nop)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 2u;
    accessor instance{ multiple };
    auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);

    uint8_t value{};
    auto pointer = &value;
    instance.set_link_(pointer);
    const auto block = pointer_cast<uint8_t>(memory);
    BOOST_REQUIRE_EQUAL(pointer, pointer_cast<uint8_t>(instance.get_link_(block)));
}

BOOST_AUTO_TEST_CASE(block_arena__get_link__unstarted__zero_filled)
{
    accessor instance{ 10 };
    instance.stack.emplace_back(link_size);
    const auto link = instance.get_link_(instance.stack.front().data());
    BOOST_REQUIRE_EQUAL(link, nullptr);
}

// capacity

BOOST_AUTO_TEST_CASE(block_arena__capacity__unstarted__zero)
{
    accessor instance{ 10 };
    BOOST_REQUIRE_EQUAL(instance.capacity_(), zero);
}

BOOST_AUTO_TEST_CASE(block_arena__capacity__started__expected)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 10u;
    constexpr auto expected = multiple * size - link_size;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_EQUAL(instance.capacity_(), expected);
    BOOST_REQUIRE_EQUAL(memory, instance.get_memory_map());
}

BOOST_AUTO_TEST_CASE(block_arena__capacity__allocated_full__zero)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 10u;
    constexpr auto overflow = multiple * size - link_size;
    accessor instance{ multiple };
    const auto memory = pointer_cast<uint8_t>(instance.start(size));
    const auto position = pointer_cast<uint8_t>(instance.allocate(overflow, 1));
    BOOST_REQUIRE_EQUAL(instance.capacity_(), zero);
    BOOST_REQUIRE_EQUAL(position, std::next(memory, link_size));
}

BOOST_AUTO_TEST_CASE(block_arena__capacity__allocated_overflow__expanded_zero)
{
    constexpr auto size = 9u;
    constexpr auto multiple = 10u;
    constexpr auto overflow = multiple * size - link_size;
    accessor instance{ multiple };
    const auto memory = pointer_cast<uint8_t>(instance.start(size));
    const auto position = pointer_cast<uint8_t>(instance.allocate(overflow, 1));
    BOOST_REQUIRE_EQUAL(instance.capacity_(), zero);
    BOOST_REQUIRE_EQUAL(position, std::next(memory, link_size));
}

// do_allocate/do_deallocate

BOOST_AUTO_TEST_CASE(block_arena__do_allocate__do_deallocate__expected)
{
    accessor instance{ 5 };
    const auto block = instance.start(10);
    BOOST_REQUIRE_NE(block, nullptr);

    constexpr auto size = 24u;
    constexpr auto offset = 4u;
    const auto memory = instance.allocate(size, offset);
    instance.deallocate(memory, size, offset);
    BOOST_REQUIRE_EQUAL(instance.deallocated_ptr, memory);
    BOOST_REQUIRE_EQUAL(instance.deallocated_size, size);
    BOOST_REQUIRE_EQUAL(instance.deallocated_offset, offset);
}

// do_is_equal

BOOST_AUTO_TEST_CASE(block_arena__do_is_equal__equal__true)
{
    accessor instance{ 42 };
    BOOST_REQUIRE(instance.is_equal(instance));
}

BOOST_AUTO_TEST_CASE(block_arena__do_is_equal__unequal__false)
{
    accessor instance1{ 42 };
    accessor instance2{ 42 };
    BOOST_REQUIRE(!instance1.is_equal(instance2));
}

BOOST_AUTO_TEST_SUITE_END()
