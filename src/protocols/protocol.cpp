/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol

using namespace std::placeholders;

// Properties.
// ----------------------------------------------------------------------------

query& protocol::archive() const NOEXCEPT
{
    return session_->archive();
}

const node::configuration& protocol::node_config() const NOEXCEPT
{
    return session_->node_config();
}

const system::settings& protocol::system_settings() const NOEXCEPT
{
    return session_->system_settings();
}

const database::settings& protocol::database_settings() const NOEXCEPT
{
    return session_->database_settings();
}

////const network::settings& protocol::network_settings() const NOEXCEPT
////{
////    return session_->network_settings();
////}

const node::settings& protocol::node_settings() const NOEXCEPT
{
    return session_->node_settings();
}

bool protocol::is_current(bool confirmed) const NOEXCEPT
{
    return session_->is_current(confirmed);
}

// Events subscription.
// ----------------------------------------------------------------------------

void protocol::subscribe_events(event_notifier&& handler) NOEXCEPT
{
    // This is a shared instance multiply-derived from network::protocol.
    const auto self = dynamic_cast<network::protocol&>(*this)
        .shared_from_sibling<node::protocol, network::protocol>();

    event_completer completer = std::bind(&protocol::handle_subscribed, self,
        _1, _2);

    session_->subscribe_events(std::move(handler),
        std::bind(&protocol::handle_subscribe,
            self, _1, _2, std::move(completer)));
}

// private
void protocol::handle_subscribe(const code& ec, object_key key,
    const event_completer& complete) NOEXCEPT
{
    // The key member is protected by one event subscription per protocol.
    BC_ASSERT_MSG(is_zero(key_), "unsafe access");

    // Protocol stop is thread safe.
    if (ec)
    {
        channel_->stop(ec);
        return;
    }

    key_ = key;
    complete(ec, key_);
}

void protocol::handle_subscribed(const code& ec, object_key key) NOEXCEPT
{
    // This is a shared instance multiply-derived from network::protocol.
    const auto self = dynamic_cast<network::protocol&>(*this)
        .shared_from_sibling<node::protocol, network::protocol>();

    boost::asio::post(channel_->strand(),
        std::bind(&protocol::subscribed, self, ec, key));
}

void protocol::subscribed(const code& ec, object_key) NOEXCEPT
{
    BC_ASSERT(channel_->stranded());

    // Unsubscriber race is ok.
    if (channel_->stopped() || ec)
        unsubscribe_events();
}

// As this has no completion handler resubscription is not allowed.
void protocol::unsubscribe_events() NOEXCEPT
{
    session_->unsubscribe_events(key_);
    key_ = {};
}

object_key protocol::events_key() const NOEXCEPT
{
    return key_;
}

} // namespace node
} // namespace libbitcoin
