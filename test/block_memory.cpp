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

#include <thread>

BOOST_AUTO_TEST_SUITE(block_memory_tests)

class accessor
  : public block_memory
{
public:
    using block_memory::block_memory;

    size_t get_count() const NOEXCEPT
    {
        return count_;
    }

    size_t get_size() const NOEXCEPT
    {
        return arenas_.size();
    }

    arena* get_arena_at(size_t index) NOEXCEPT
    {
        return &arenas_.at(index);
    }
};

BOOST_AUTO_TEST_CASE(block_memory__get_arena__no_multiple_no_threads__default_arena)
{
    constexpr size_t multiple = 0;
    constexpr size_t threads = 0;
    accessor instance{ multiple, threads };
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_arena(), default_arena::get());
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__no_threads__default_arena)
{
    constexpr size_t multiple = 42;
    constexpr size_t threads = 0;
    accessor instance{ multiple, threads };
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_arena(), default_arena::get());
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__no_multiple__default_arena)
{
    constexpr size_t multiple = 0;
    constexpr size_t threads = 1;
    accessor instance{ multiple, threads };
    BOOST_REQUIRE_EQUAL(instance.get_size(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_EQUAL(instance.get_arena(), default_arena::get());
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__multiple_one_thread__not_default_arena)
{
    constexpr size_t multiple = 42;
    constexpr size_t threads = 1;
    accessor instance{ multiple, threads };
    BOOST_REQUIRE_EQUAL(instance.get_size(), one);
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_NE(instance.get_arena(), default_arena::get());
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__multiple_threads__count_unincremented)
{
    constexpr size_t multiple = 42;
    constexpr size_t threads = 2;
    accessor instance{ multiple, threads };
    BOOST_REQUIRE_EQUAL(instance.get_size(), two);

    // On any given thread count must not change.
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_NE(instance.get_arena(), default_arena::get());
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
    BOOST_REQUIRE_NE(instance.get_arena(), default_arena::get());
    BOOST_REQUIRE_EQUAL(instance.get_count(), zero);
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__two_threads__independent_not_default_arenas)
{
    // non-zero multiple ensures block_arenas but value is otherwise unimportant.
    constexpr size_t multiple = 42;
    constexpr size_t threads = 2;
    accessor instance{ multiple, threads };
    size_t count1{};
    size_t count2{};
    void* arena1{};
    void* arena2{};

    std::thread thread1([&]() NOEXCEPT
    {
        count1 = instance.get_count();
        arena1 = instance.get_arena();

        std::thread thread2([&]() NOEXCEPT
        {
            count2 = instance.get_count();
            arena2 = instance.get_arena();
        });
            
        thread2.join();
    });

    thread1.join();

    BOOST_REQUIRE_NE(arena2, default_arena::get());
    BOOST_REQUIRE_NE(arena1, default_arena::get());
    BOOST_REQUIRE_NE(arena1, arena2);
    BOOST_REQUIRE_NE(count1, count2);
}

BOOST_AUTO_TEST_CASE(block_memory__get_arena__overflow_threads__default_arena)
{
    constexpr size_t multiple = 42;
    constexpr size_t threads = 2;
    accessor instance{ multiple, threads };
    size_t count1a{};
    size_t count1b{};
    size_t count2a{};
    size_t count2b{};
    size_t count3a{};
    size_t count3b{};
    void* arena1{};
    void* arena2{};
    void* arena3{};

    // order is required to ensure third is the overflow.
    std::thread thread1([&]() NOEXCEPT
    {
        count1a = instance.get_count();
        arena1 = instance.get_arena();
        count1b = instance.get_count();

        std::thread thread2([&]() NOEXCEPT
        {
            count2a = instance.get_count();
            arena2 = instance.get_arena();
            count2b = instance.get_count();

            std::thread thread3([&]() NOEXCEPT
            {
                count3a = instance.get_count();
                arena3 = instance.get_arena();
                count3b = instance.get_count();
            });

            thread3.join();
        });

        thread2.join();
    });

    thread1.join();

    // Arenas are ordered by thread order above.
    BOOST_REQUIRE_EQUAL(arena1, instance.get_arena_at(0));
    BOOST_REQUIRE_EQUAL(arena2, instance.get_arena_at(1));
    BOOST_REQUIRE_NE(arena1, arena2);

    // Index overflows to default arena.
    BOOST_REQUIRE_EQUAL(arena3, default_arena::get());

    // Count reflects total, not the index of the thread.
    BOOST_REQUIRE_EQUAL(count1a, 0u);
    BOOST_REQUIRE_EQUAL(count2a, 1u);
    BOOST_REQUIRE_EQUAL(count3a, 2u);
    BOOST_REQUIRE_EQUAL(count1b, 1u);
    BOOST_REQUIRE_EQUAL(count2b, 2u);
    BOOST_REQUIRE_EQUAL(count3b, 3u);
}

BOOST_AUTO_TEST_SUITE_END()
