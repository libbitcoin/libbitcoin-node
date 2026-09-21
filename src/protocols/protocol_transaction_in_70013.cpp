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
#include <bitcoin/node/protocols/protocol_transaction_in_70013.hpp>

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_in_70013

using namespace system;

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Submission.
// ----------------------------------------------------------------------------

// bip133: the peer is sent our rate when current and the maximum otherwise, so
// a tx below the rate, or any tx while suspended, is sent against instruction.
void protocol_transaction_in_70013::do_handle_submit(const code& ec,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || ec == network::error::service_stopped)
        return;

    if (ec == error::insufficient_fee || ec == error::pooling_disabled)
    {
        LOGR("Tx from [" << opposite() << "] " << ec.message());
        stop(ec);
        return;
    }

    protocol_transaction_in_70001::do_handle_submit(ec, gate);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
