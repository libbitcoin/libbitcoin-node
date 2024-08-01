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

#include <shared_mutex>
#include <bitcoin/network.hpp>
#include <bitcoin/node/block_arena.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API block_memory final
  : public network::memory
{
public:
    block_memory() NOEXCEPT;

    arena* get_arena() NOEXCEPT override;
    retainer::ptr get_retainer() NOEXCEPT override;

private:
    block_arena arena_{};
    std::shared_mutex mutex_{};
};

} // namespace node
} // namespace libbitcoin

#endif
