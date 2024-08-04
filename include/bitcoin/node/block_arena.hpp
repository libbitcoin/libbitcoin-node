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
#ifndef LIBBITCOIN_NODE_BLOCK_ARENA_HPP
#define LIBBITCOIN_NODE_BLOCK_ARENA_HPP

#include <shared_mutex>
#include <bitcoin/system.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Thread UNSAFE linear memory arena.
class BCN_API block_arena final
  : public arena
{
public:
    DELETE_COPY(block_arena);
    
    block_arena(size_t size=zero) NOEXCEPT;
    block_arena(block_arena&& other) NOEXCEPT;
    ~block_arena() NOEXCEPT;

    block_arena& operator=(block_arena&& other) NOEXCEPT;

    /// Get memory block retainer mutex.
    inline std::shared_mutex& get_mutex() NOEXCEPT
    {
        return mutex_;
    }

private:
    void* do_allocate(size_t bytes, size_t align) THROWS override;
    void do_deallocate(void* ptr, size_t bytes, size_t align) NOEXCEPT override;
    bool do_is_equal(const arena& other) const NOEXCEPT override;
    size_t do_get_capacity() const NOEXCEPT override;

    // These are thread safe.
    std::shared_mutex mutex_{};
    uint8_t* memory_map_;
    size_t capacity_;

    // This is unprotected, caller must guard.
    size_t offset_;

};

} // namespace node
} // namespace libbitcoin

#endif
