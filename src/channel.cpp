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
#include <bitcoin/node/channel.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// TODO: this can be optimized using a circular cuckoo filter. Minimal false
// positives are acceptable. Elements should be removed from filter on read,
// since received objects that are already stored are not again announced.

// Capture configured buffer size.
channel::channel(memory& memory, const logger& log, const socket::ptr& socket,
    const node::configuration& config, uint64_t identifier, bool quiet) NOEXCEPT
  : network::channel(memory, log, socket, config.network, identifier, quiet),
    announced_(config.node.announcement_cache)
{
}

void channel::set_announced(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());
    announced_.push_back(hash);
}

bool channel::was_announced(const hash_digest& hash) const NOEXCEPT
{
    BC_ASSERT(stranded());
    return std::find(announced_.begin(), announced_.end(), hash) !=
        announced_.end();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
