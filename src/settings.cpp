/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/settings.hpp>

#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::asio;

settings::settings()
  : maximum_deviation(1.5),
    block_latency_seconds(5),
    refresh_transactions(false)
{
}

// There are no current distinctions spanning chain contexts.
settings::settings(config::settings)
  : settings()
{
}

duration settings::block_latency() const
{
    return seconds(block_latency_seconds);
}

} // namespace node
} // namespace libbitcoin
