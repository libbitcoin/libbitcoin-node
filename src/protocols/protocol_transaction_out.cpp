/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_transaction_out.hpp>

#include <cstddef>
#include <functional>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "transaction"
#define CLASS protocol_transaction_out
    
using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_transaction_out::protocol_transaction_out(p2p& network,
    channel::ptr channel, block_chain& blockchain, transaction_pool& pool)
  : protocol_events(network, channel, NAME),
    blockchain_(blockchain),
    pool_(pool),
    relay_to_peer_(peer_version().relay),
    CONSTRUCT_TRACK(protocol_transaction_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_transaction_out::start()
{
    if (relay_to_peer_)
    {
        // Subscribe to transaction pool notifications and relay txs.
        pool_.subscribe_transaction(BIND3(handle_floated, _1, _2, _3));
    }
}

// Subscription.
//-----------------------------------------------------------------------------

bool protocol_transaction_out::handle_floated(const code& ec,
    const index_list&, const chain::transaction& transaction)
{
    ///////////////////////////////////////////////////////////////////////////
    // TODO: add host id to message subscriber to avoid tx reflection.
    // TODO: implement this handler.
    ///////////////////////////////////////////////////////////////////////////
    return true;
}

} // namespace node
} // namespace libbitcoin
