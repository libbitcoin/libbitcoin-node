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
        stack.emplace_back(bytes);
        return stack.back().data();
    }

    void free(void* address) NOEXCEPT override
    {
        freed.push_back(address);
    }

    void push_(size_t minimum=zero) THROWS
    {
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

    // These can be directly invoked via public methods.
    //void* do_allocate(size_t bytes, size_t align) THROWS override;
    //void do_deallocate(void* ptr, size_t bytes, size_t align) NOEXCEPT override;
    //bool do_is_equal(const arena& other) const NOEXCEPT override;

    data_stack stack{};
    std::vector<void*> freed{};
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

BOOST_AUTO_TEST_CASE(block_arena__start__zero__minimal_allocation)
{
    constexpr auto size = zero;
    constexpr auto multiple = 42u;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_NE(memory, nullptr);
    BOOST_REQUIRE_LT(size, sizeof(void*));
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    BOOST_REQUIRE_EQUAL(instance.stack.front().size(), sizeof(void*));
}

BOOST_AUTO_TEST_CASE(block_arena__start__at_least_pointer_size__expected)
{
    constexpr auto size = 42u;
    constexpr auto multiple = 10u;
    accessor instance{ multiple };
    const auto memory = instance.start(size);
    BOOST_REQUIRE_NE(memory, nullptr);
    BOOST_REQUIRE_GE(size, sizeof(void*));
    BOOST_REQUIRE_EQUAL(instance.stack.size(), one);
    BOOST_REQUIRE_EQUAL(instance.stack.front().size(), size * multiple);
    BOOST_REQUIRE_EQUAL(instance.stack.front().data(), instance.get_memory_map());
}

// detach

BOOST_AUTO_TEST_CASE(block_arena__detach__unstarted__zero)
{
    accessor instance{ 42 };
    const auto size = instance.detach();
    BOOST_REQUIRE_EQUAL(size, zero);
}

// release

BOOST_AUTO_TEST_CASE(block_arena__release__nullptr__nop)
{
    accessor instance{ 42 };
    instance.release(nullptr);
}

// to_aligned

BOOST_AUTO_TEST_CASE(block_arena__to_aligned__zero_one__zero)
{
    constexpr auto aligned = accessor::to_aligned_(0, 1);
    BOOST_REQUIRE_EQUAL(aligned, zero);
}

// push

BOOST_AUTO_TEST_CASE(block_arena__push__)
{
    accessor instance{ 42 };
    instance.push_();
}

// set_link

BOOST_AUTO_TEST_CASE(block_arena__set_link__nullptr__nop)
{
    accessor instance{ 42 };
    instance.set_link_(nullptr);
}

// get_link

BOOST_AUTO_TEST_CASE(block_arena__get_link__unstarted__zero_filled)
{
    accessor instance{ 42 };
    instance.stack.emplace_back(sizeof(void*));
    const auto link = instance.get_link_(instance.stack.front().data());
    BOOST_REQUIRE_EQUAL(link, nullptr);
}

// capacity

BOOST_AUTO_TEST_CASE(block_arena__capacity__unstarted__zero)
{
    accessor instance{ 42 };
    const auto capacity = instance.capacity_();
    BOOST_REQUIRE_EQUAL(capacity, zero);
}

// do_allocate/do_deallocate

BOOST_AUTO_TEST_CASE(block_arena__do_allocate__do_deallocate__expected)
{
    accessor instance{ 42 };
    const auto block = instance.start(0);
    BOOST_REQUIRE_NE(block, nullptr);

    const auto memory = instance.allocate(24, 4);
    instance.deallocate(memory, 24, 4);
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
