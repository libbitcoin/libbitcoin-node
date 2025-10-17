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
#include <bitcoin/node/protocols/protocol_peer.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_peer

using namespace system;
using namespace network;
using namespace std::placeholders;

// Organizers.
// ----------------------------------------------------------------------------

void protocol_peer::organize(const system::chain::header::cptr& header,
    organize_handler&& handler) NOEXCEPT
{
    session_->organize(header, std::move(handler));
}

void protocol_peer::organize(const system::chain::block::cptr& block,
    organize_handler&& handler) NOEXCEPT
{
    session_->organize(block, std::move(handler));
}

void protocol_peer::get_hashes(map_handler&& handler) NOEXCEPT
{
    session_->get_hashes(std::move(handler));
}

void protocol_peer::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    session_->put_hashes(map, std::move(handler));
}

// Methods.
// ----------------------------------------------------------------------------

void protocol_peer::performance(uint64_t speed,
    network::result_handler&& handler) const NOEXCEPT
{
    // Passed protocol->session->full_node->check_chaser.post->do_update.
    session_->performance(key_, speed, std::move(handler));
}

code protocol_peer::fault(const code& ec) NOEXCEPT
{
    // Short-circuit self stop.
    stop(ec);

    // Stop all other channels and suspend all connectors/acceptors.
    session_->fault(ec);
    return ec;
}

// Announcements.
// ----------------------------------------------------------------------------

void protocol_peer::set_announced(const system::hash_digest& hash) NOEXCEPT
{
    channel_->set_announced(hash);
}

bool protocol_peer::was_announced(const system::hash_digest& hash) const NOEXCEPT
{
    return channel_->was_announced(hash);
}

// Events notification.
// ----------------------------------------------------------------------------

void protocol_peer::notify(const code& ec, chase event_,
    event_value value) const NOEXCEPT
{
    session_->notify(ec, event_, value);
}

void protocol_peer::notify_one(object_key key, const code& ec, chase event_,
    event_value value) const NOEXCEPT
{
    session_->notify_one(key, ec, event_, value);
}

// Events subscription.
// ----------------------------------------------------------------------------

void protocol_peer::subscribe_events(event_notifier&& handler) NOEXCEPT
{
    event_completer completer = BIND(handle_subscribed, _1, _2);
    session_->subscribe_events(std::move(handler),
        BIND(handle_subscribe, _1, _2, std::move(completer)));
}

// private
void protocol_peer::handle_subscribe(const code& ec, object_key key,
    const event_completer& complete) NOEXCEPT
{
    // The key member is protected by one event subscription per protocol.
    BC_ASSERT_MSG(is_zero(key_), "unsafe access");

    // Protocol stop is thread safe.
    if (ec)
    {
        stop(ec);
        return;
    }

    key_ = key;
    complete(ec, key_);
}

void protocol_peer::handle_subscribed(const code& ec, object_key key) NOEXCEPT
{
    POST(subscribed, ec, key);
}

void protocol_peer::subscribed(const code& ec, object_key) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Unsubscriber race is ok.
    if (stopped(ec))
        unsubscribe_events();
}

// As this has no completion handler resubscription is not allowed.
void protocol_peer::unsubscribe_events() NOEXCEPT
{
    session_->unsubscribe_events(key_);
    key_ = {};
}

object_key protocol_peer::events_key() const NOEXCEPT
{
    return key_;
}

} // namespace node
} // namespace libbitcoin
