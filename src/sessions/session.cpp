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

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session

using namespace system::chain;
using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

session::session(full_node& node) NOEXCEPT
  : node_(node)
{
}

session::~session() NOEXCEPT
{
}

// Organizers.
// ----------------------------------------------------------------------------

void session::organize(const header::cptr& header,
    organize_handler&& handler) NOEXCEPT
{
    node_.organize(header, std::move(handler));
}

void session::organize(const block::cptr& block,
    organize_handler&& handler) NOEXCEPT
{
    node_.organize(block, std::move(handler));
}

void session::get_hashes(map_handler&& handler) NOEXCEPT
{
    node_.get_hashes(std::move(handler));
}

void session::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    node_.put_hashes(map, std::move(handler));
}

// Events.
// ----------------------------------------------------------------------------

void session::notify(const code& ec, chase event_, event_link value) NOEXCEPT
{
    node_.notify(ec, event_, value);
}

void session::async_subscribe_events(event_handler&& handler) NOEXCEPT
{
    // This is necessary because of multiple inheritance (see attach<Session>).
    const auto self = std::dynamic_pointer_cast<node::session>(
        dynamic_cast<network::session*>(this)->shared_from_this());

    boost::asio::post(node_.strand(),
        std::bind(&session::subscribe_events,
            self, std::move(handler)));
}

// protected
void session::subscribe_events(const event_handler& handler) NOEXCEPT
{
    BC_ASSERT(node_.stranded());
    node_.subscribe_events(move_copy(handler));
}

// Methods.
// ----------------------------------------------------------------------------

void session::performance(uint64_t, uint64_t, result_handler&& handler) NOEXCEPT
{
    // This session type does not implement performance, handler error.
    // The handler captures the protocol shared pointer for its lifetime.
    // This session lifetime is not required beyond return from this method.
    boost::asio::post(node_.strand(),
        std::bind(handler, system::error::not_implemented));
}

// Properties.
// ----------------------------------------------------------------------------

query& session::archive() const NOEXCEPT
{
    return node_.archive();
}

const configuration& session::config() const NOEXCEPT
{
    return node_.config();
}

bool session::is_current() const NOEXCEPT
{
    return node_.is_current();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
