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

// The fee filter is expressed in satoshis per virtual kilobyte (bip133).
constexpr uint64_t vbytes_per_vkbyte = 1'000;

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

    // bip133: the peer does not announce a tx below our configured rate.
    if (const auto minimum = node_settings().minimum_fee_rate_();
        !is_zero(minimum))
    {
        SEND(fee_filter{ minimum }, handle_send, _1);
    }

    protocol_transaction_out_70001::start();
}

// Inbound (feefilter).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_70013::handle_receive_fee_filter(const code& ec,
    const fee_filter::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    minimum_fee_ = message->minimum_fee;
    return true;
}

// Outbound (inv).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_70013::do_announce(transaction_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return false;

    // The tx is archived and its prevouts populated, so the rate is known.
    database::fee_rate rate{};
    if (!archive().get_tx_fees(rate, link))
    {
        fault(database::error::integrity);
        return false;
    }

    // bip133: the peer is not sent a tx below the rate it advertised.
    if (insufficient(rate))
        return true;

    return protocol_transaction_out_70001::do_announce(link);
}

// private
bool protocol_transaction_out_70013::insufficient(
    const database::fee_rate& rate) const NOEXCEPT
{
    return ceilinged_multiply(rate.fee, vbytes_per_vkbyte) <
        ceilinged_multiply(minimum_fee_,
            possible_wide_cast<uint64_t>(rate.bytes));
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
