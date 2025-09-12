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
    
using namespace system;
using namespace network::messages::p2p;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_header_out_70012::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is deferred, in handle_receive_send_headers.
    SUBSCRIBE_CHANNEL(send_headers, handle_receive_send_headers, _1, _2);
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
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::block:
        {
            // value is organized block pk.
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_announce, std::get<header_t>(value));
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Outbound (headers).
// ----------------------------------------------------------------------------

bool protocol_header_out_70012::do_announce(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (stopped())
        return false;

    // Don't announce to peer that announced to us.
    const auto hash = query.get_header_key(link);
    if (was_announced(hash))
    {
        LOGP("Suppress " << encode_hash(hash) << " to ["
            << authority() << "].");
        return true;
    }

    const auto ptr = query.get_header(link);
    if (!ptr)
    {
        ////stop(fault(system::error::not_found));
        LOGF("Organized header not found.");
        return true;
    }

    LOGN("Announce " << encode_hash(hash) << " to [" << authority() << "].");
    SEND(headers{ { ptr } }, handle_send, _1);
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

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));
    return false;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
