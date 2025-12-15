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
#ifndef LIBBITCOIN_NODE_INTERFACES_BITCOIND_REST_HPP
#define LIBBITCOIN_NODE_INTERFACES_BITCOIND_REST_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/interfaces/types.hpp>

namespace libbitcoin {
namespace node {
namespace interface {

struct bitcoind_rest_methods
{
    static constexpr std::tuple methods
    {
        method<"rest1">{},
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using rest1 = at<0>;
};

} // namespace interface
} // namespace node
} // namespace libbitcoin

#endif
