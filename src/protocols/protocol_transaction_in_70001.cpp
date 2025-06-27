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
#include <bitcoin/node/protocols/protocol_transaction_in_70001.hpp>

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_in_70001

using namespace network::messages;

// Inbound (inv).
// ----------------------------------------------------------------------------

bool protocol_transaction_in_70001::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // bip144: get_data uses witness constant but inv does not.
    const auto tx_count = message->count(type_id::transaction);

    // Many satoshi v25.0 and v25.1 peers fail to honor version.relay = 0.
    if (!relay_ && !is_zero(tx_count))
    {
        LOGR("Unrequested txs (" << tx_count << ") from ["
            << authority() << "] " << peer_version()->user_agent);

        stop(network::error::protocol_violation);
        return false;
    }

    return protocol_transaction_in_106::handle_receive_inventory(ec, message);
}

} // namespace node
} // namespace libbitcoin
