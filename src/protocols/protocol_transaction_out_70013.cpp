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
#include <bitcoin/node/protocols/protocol_transaction_out_70013.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_70013

using namespace system;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start
// ----------------------------------------------------------------------------

void protocol_transaction_out_70013::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(fee_filter, handle_receive_fee_filter, _1, _2);
    protocol_transaction_out_70001::start();
    do_send_fee_filter();
}

// Outbound (feefilter).
// ----------------------------------------------------------------------------

// bip133: a relaying channel is sent every tx (coverage decides admission),
// and one with no relay slot, or while not current, is sent nothing.
void protocol_transaction_out_70013::do_send_fee_filter() NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto relaying = relay_slot() < relay_slots && is_current_chain(true);
    const auto minimum = relaying ? zero : system_settings().max_money();

    if (minimum == sent_fee_)
        return;

    sent_fee_ = minimum;
    NOTIFY(fee_filter{ minimum }, handle_send, _1);

    // bip35: the reply follows the zero filter, so it is not filtered.
    if (relaying && !requested_ && request_mempool())
    {
        requested_ = true;
        NOTIFY(memory_pool{}, handle_send, _1);
    }
}

bool protocol_transaction_out_70013::handle_chase(const code& ec,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (to_chase(value))
    {
        case chase::block:
        case chase::stale:
        {
            POST(do_send_fee_filter);
            break;
        }
        default:
        {
            break;
        }
    }

    return protocol_transaction_out_70001::handle_chase(ec, value);
}

// Inbound (feefilter).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_70013::handle_receive_fee_filter(const code& ec,
    const fee_filter::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    set_minimum_fee(message->minimum_fee);
    set_relay_rate(message->minimum_fee);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
