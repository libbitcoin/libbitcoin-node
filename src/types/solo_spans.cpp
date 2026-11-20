/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/node/types/solo_spans.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

solo_spans::ticket solo_spans::begin() NOEXCEPT
{
    return state_.fetch_add(one_begun + one_open);
}

// Alone if none was open at its begin and the state is as its begin left it.
bool solo_spans::end(ticket begun) NOEXCEPT
{
    const auto ending = state_.fetch_sub(one_open);
    return is_zero(begun % one_begun) &&
        (ending == begun + one_begun + one_open);
}

} // namespace node
} // namespace libbitcoin
