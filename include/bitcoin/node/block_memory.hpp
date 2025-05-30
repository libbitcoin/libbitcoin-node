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
#ifndef LIBBITCOIN_NODE_BLOCK_MEMORY_HPP
#define LIBBITCOIN_NODE_BLOCK_MEMORY_HPP

#include <atomic>
#include <bitcoin/network.hpp>
#include <bitcoin/node/block_arena.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Thread SAFE linked-linear arena allocator.
class BCN_API block_memory
  : public network::memory
{
public:
    DELETE_COPY_MOVE_DESTRUCT(block_memory);

    /// Per thread multiple of wire size for each linear allocation chunk.
    /// Returns default_arena if multiple is zero or threads exceeded.
    block_memory(size_t multiple, size_t threads) NOEXCEPT;

    /// Each thread obtains an arena.
    arena* get_arena() NOEXCEPT override;

protected:
    // This is thread safe.
    std::atomic_size_t count_{ zero };

    // This is protected by constructor init and thread_local indexation.
    std::vector<block_arena> arenas_{};
};

} // namespace node
} // namespace libbitcoin

#endif
