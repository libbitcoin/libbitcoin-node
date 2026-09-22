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
#include <bitcoin/node/protocols/protocol_observer.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_observer

using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_observer::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_chase(BIND(handle_chase, _1, _2));

    if (relay_disallowed_)
    {
        SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
    }

    SUBSCRIBE_BROADCAST(network::diagnostics, handle_broadcast_diagnostics, _1, _2, _3);
    SUBSCRIBE_BROADCAST(network::terminator, handle_broadcast_terminator, _1, _2, _3);
    protocol_peer::start();
}

void protocol_observer::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_chase();
    protocol_peer::stopping(ec);
}

// handle events (suspend)
// ----------------------------------------------------------------------------

bool protocol_observer::handle_chase(const code& ec, event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (to_chase(value))
    {
        case chase::suspend:
        {
            // The code distinguishes a stop from a drop (manual removal).
            stop(ec);
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Inbound (inv).
// ----------------------------------------------------------------------------
// Protocol hygiene for messages that may not be captured.

bool protocol_observer::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    //  Common with Satoshi 25.0 and 25.1.
    if (relay_disallowed_ && message->any_transaction())
    {
        LOGR("Unrequested tx relay from [" << opposite() << "] "
            << peer_version()->user_agent);

        stop(network::error::protocol_violation);
        return false;
    }

    return true;
}

// Diagnostics (capture).
// ----------------------------------------------------------------------------

network::diagnostics::target protocol_observer::group() const NOEXCEPT
{
    return group_;
}

bool protocol_observer::handle_broadcast_diagnostics(const code& ec,
    const network::diagnostics::cptr& message, uint64_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!message->targets(identifier()) && !message->targets(group()))
        return true;

    using namespace system;
    const auto peer = peer_version();
    uint64_t services{ service::node_none };
    network::config::address local{};
    int64_t time_offset{};
    std::string agent{};
    bool relay{};

    if (peer)
    {
        time_offset = subtract<int64_t>(peer->timestamp, created());
        local = { peer->address_receiver };
        services = peer->services;
        agent = peer->user_agent;
        relay = peer->relay;
    }

    message->add(
    {
        .group = group(),
        .identifier = identifier(),
        .endpoint = opposite(),
        .address = outbound(),
        .local = local,
        .binding = binding(),

        .encrypted = encrypted(),
        .peer_relay = relay,
        .peer_start_height = start_height(),
        .peer_version = negotiated_version(),
        .peer_services = services,
        .peer_minimum_fee = minimum_fee(),
        .peer_user_agent = agent,

        .created = created(),
        .last_read = last_read(),
        .last_write = last_write(),
        .time_offset = time_offset,
        .bytes_sent = sent(),
        .bytes_received = received(),
        .bytes_sent_by_message = sent_by_message(),
        .bytes_received_by_message = received_by_message(),

        .ping_time = ping_time(),
        .minimum_ping_time = minimum_ping_time(),
        .pending_ping_time = pending_ping_time()
    });

    return true;
}

// The first member stops, so the round completes without the other channels.
bool protocol_observer::handle_broadcast_terminator(const code& ec,
    const network::terminator::cptr& message, uint64_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!message->targets(identifier(), outbound(), opposite()))
        return true;

    message->stopped();
    stop(message->reason());
    return false;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
