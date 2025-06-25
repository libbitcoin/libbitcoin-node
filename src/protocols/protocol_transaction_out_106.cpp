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
#include <bitcoin/node/protocols/protocol_transaction_out_106.hpp>

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_106

using namespace network::messages;
using namespace std::placeholders;

// start/stop
// ----------------------------------------------------------------------------

void protocol_transaction_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));
    protocol::start();
}

void protocol_transaction_out_106::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (transaction)
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::transaction:
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

} // namespace node
} // namespace libbitcoin
