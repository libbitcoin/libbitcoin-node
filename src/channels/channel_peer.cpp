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
#include <bitcoin/node/channels/channel_peer.hpp>

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void channel_peer::set_announced(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());
    announced_.insert(hash);
}

bool channel_peer::was_announced(const hash_digest& hash) const NOEXCEPT
{
    BC_ASSERT(stranded());
    return announced_.contains(hash);
}

void channel_peer::set_downloader(downloader&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    downloader_ = std::move(handler);
}

void channel_peer::set_relay_slot(size_t slot) NOEXCEPT
{
    relay_slot_.store(slot);
}

size_t channel_peer::relay_slot() const NOEXCEPT
{
    return relay_slot_.load();
}

bool channel_peer::download(const map_ptr& map,
    const job::ptr& job) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!downloader_)
        return false;

    downloader_(map, job);
    return true;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
