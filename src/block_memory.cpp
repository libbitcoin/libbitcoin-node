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
#include <bitcoin/node/block_memory.hpp>

#include <memory>
#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace node {

arena* block_memory::get_arena() NOEXCEPT
{
    return &arena_;
}

retainer::ptr block_memory::get_retainer() NOEXCEPT
{
    BC_PUSH_WARNING(NO_NEW_OR_DELETE)
    return std::make_shared<retainer>(arena_.get_mutex());
    BC_POP_WARNING()
}

} // namespace node
} // namespace libbitcoin
