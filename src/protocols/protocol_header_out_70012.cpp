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
#include <bitcoin/node/protocols/protocol_header_out_70012.hpp>

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_out_70012
    
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_header_out_70012::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(send_headers, handle_receive_send_headers, _1, _2);

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));
    protocol_header_out_31800::start();
}

void protocol_header_out_70012::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (block)
// ----------------------------------------------------------------------------

bool protocol_header_out_70012::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::organized:
        {
            // TODO:
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Inbound (send_headers).
// ----------------------------------------------------------------------------

bool protocol_header_out_70012::handle_receive_send_headers(const code& ec,
    const send_headers::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    SUBSCRIBE_BROADCAST(block, handle_broadcast_block, _1, _2, _3);
    return false;
}

// Outbound (headers).
// ----------------------------------------------------------------------------

bool protocol_header_out_70012::handle_broadcast_block(const code& ec,
    const block::cptr& message, uint64_t sender) NOEXCEPT
{
    BC_ASSERT(stranded());

    // see: protocol_block_out_70012
    ////// Ignore desubscription from other protocols (block_out).
    ////if (ec == network::error::desubscribed)
    ////    return true;

    if (stopped(ec))
        return false;

    if (sender == identifier())
        return true;

    SEND(headers{ { message->block_ptr->header_ptr() } }, handle_send, _1);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
