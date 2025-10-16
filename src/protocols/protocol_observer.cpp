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
#include <bitcoin/node/protocols/protocol_observer.hpp>

#include <bitcoin/network.hpp>
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
    subscribe_events(BIND(handle_event, _1, _2, _3));

    if (relay_disallowed_)
    {
        SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
    }

    ////SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    protocol_peer::start();
}

void protocol_observer::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol_peer::stopping(ec);
}

// handle events (suspend)
// ----------------------------------------------------------------------------

bool protocol_observer::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::suspend:
        {
            stop(error::suspended_channel);
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

// Inbound (inv, get_data).
// ----------------------------------------------------------------------------
// These implement protocol hygiene for messages that may not be captured.
// This also allows various protocols to not have to handle all conditions.
// Not currently comprehensive but at least catches a lot of disallowed relay.

bool protocol_observer::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    //  Common with Satoshi 25.0 and 25.1.
    if (relay_disallowed_ && message->any_transaction())
    {
        LOGR("Unrequested tx relay from [" << authority() << "] "
            << peer_version()->user_agent);

        stop(network::error::protocol_violation);
        return false;
    }

    ////// Witness types never allowed in inventory (wxtid excluded).
    ////if (message->any_witness())
    ////{
    ////    LOGR("Unsupported witness inventory from [" << authority() << "].");
    ////    stop(network::error::protocol_violation);
    ////    return false;
    ////}

    return true;
}

////bool protocol_observer::handle_receive_get_data(const code& ec,
////    const get_data::cptr& message) NOEXCEPT
////{
////    BC_ASSERT(stranded());
////
////    if (stopped(ec))
////        return false;
////
////    // Witness types only allowed in get_data if witness service advertised.
////    if (!node_witness_ && message->any_witness())
////    {
////        LOGR("Unsupported witness get_data from [" << authority() << "].");
////        stop(network::error::protocol_violation);
////        return false;
////    }
////
////    return true;
////}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
