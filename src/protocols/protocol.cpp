/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

void protocol::subscribe_poll(full_node::poll_notifier&& handler) const NOEXCEPT
{
    session_.subscribe_poll(identifier(), std::move(handler));
}

void protocol::unsubscribe_poll() const NOEXCEPT
{
    session_.unsubscribe_poll(identifier());
}

const configuration& protocol::config() const NOEXCEPT
{
    return session_.config();
}

full_node::query& protocol::archive() const NOEXCEPT
{
    return session_.archive();
}

} // namespace node
} // namespace libbitcoin
