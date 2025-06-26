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

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start/stop
// ----------------------------------------------------------------------------

void protocol_transaction_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // TODO: protocol_transaction_out_70001.
    // Events subscription is asynchronous, events may be missed.
    if (peer_version()->relay)
        subscribe_events(BIND(handle_event, _1, _2, _3));

    protocol::start();
}

void protocol_transaction_out_106::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // TODO: protocol_transaction_out_70001.
    // Unsubscriber race is ok.
    if (peer_version()->relay)
        unsubscribe_events();

    protocol::stopping(ec);
}

// handle events (transaction)
// ----------------------------------------------------------------------------

bool protocol_transaction_out_106::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::transaction:
        {
            // value is organized tx pk.
            BC_ASSERT(std::holds_alternative<transaction_t>(value));
            POST(do_organized, std::get<transaction_t>(value));
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

bool protocol_transaction_out_106::do_organized(transaction_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (stopped())
        return false;

    ///////////////////////////////////////////////////////////////////////////
    // TODO: don't send to peer that sent to us.
    ///////////////////////////////////////////////////////////////////////////

    // Compute/use witness hash if flagged.
    const inventory inv{ { { tx_type_, query.get_tx_key(link) } } };
    SEND(inv, handle_send, _1);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
