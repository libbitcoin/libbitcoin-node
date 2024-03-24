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

namespace libbitcoin {
namespace node {

using namespace network;

protocol::~protocol() NOEXCEPT
{
}

// Organizers.
// ----------------------------------------------------------------------------

void protocol::organize(const system::chain::header::cptr& header,
    organize_handler&& handler) NOEXCEPT
{
    session_.organize(header, std::move(handler));
}

void protocol::organize(const system::chain::block::cptr& block,
    organize_handler&& handler) NOEXCEPT
{
    session_.organize(block, std::move(handler));
}

void protocol::get_hashes(map_handler&& handler) NOEXCEPT
{
    session_.get_hashes(std::move(handler));
}

void protocol::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    session_.put_hashes(map, std::move(handler));
}

// Events.
// ----------------------------------------------------------------------------

void protocol::notify(const code& ec, chase event_, event_link value) NOEXCEPT
{
    session_.notify(ec, event_, value);
}

void protocol::async_subscribe_events(event_handler&& handler) NOEXCEPT
{
    return session_.async_subscribe_events(std::move(handler));
}

// Methods.
// ----------------------------------------------------------------------------

void protocol::performance(uint64_t channel, uint64_t speed,
    network::result_handler&& handler) const NOEXCEPT
{
    session_.performance(channel, speed, std::move(handler));
}

// Properties.
// ----------------------------------------------------------------------------

query& protocol::archive() const NOEXCEPT
{
    return session_.archive();
}

const configuration& protocol::config() const NOEXCEPT
{
    return session_.config();
}

bool protocol::is_current() const NOEXCEPT
{
    return session_.is_current();
}

} // namespace node
} // namespace libbitcoin
