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
#ifndef LIBBITCOIN_NODE_BLOCK_MEMORY_HPP
#define LIBBITCOIN_NODE_BLOCK_MEMORY_HPP

#include <atomic>
#include <shared_mutex>
#include <thread>
#include <bitcoin/network.hpp>
#include <bitcoin/node/block_arena.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Thread SAFE linear memory allocation and tracking.
class BCN_API block_memory final
  : public network::memory
{
public:
    DELETE_COPY_MOVE_DESTRUCT(block_memory);

    /// Default allocate each arena to preclude allcation and locking.
    block_memory(size_t bytes, size_t threads) NOEXCEPT;

    /// Each thread obtains an arena of the same size.
    arena* get_arena() NOEXCEPT override;

    /// Each thread obtains its arena's retainer.
    retainer::ptr get_retainer() NOEXCEPT override;

protected:
    block_arena* get_block_arena() THROWS;

private:
    // This is thread safe.
    std::atomic_size_t count_;

    // This is protected by constructor init and thread_local indexation.
    std::vector<block_arena> arenas_;
};

} // namespace node
} // namespace libbitcoin

#endif
