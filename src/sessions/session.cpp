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
#include <bitcoin/node/sessions/session.hpp>

#include <utility>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

session::session(full_node& node) NOEXCEPT
  : node_(node)
{
}

void session::subscribe_poll(uint64_t key,
    full_node::poll_notifier&& handler) const NOEXCEPT
{
    node_.subscribe_poll(key, std::move(handler));
}

void session::unsubscribe_poll(uint64_t key) const NOEXCEPT
{
    node_.unsubscribe_poll(key);
}

const configuration& session::config() const NOEXCEPT
{
    return node_.config();
}

full_node::query& session::archive() const NOEXCEPT
{
    return node_.archive();
}

} // namespace node
} // namespace libbitcoin
