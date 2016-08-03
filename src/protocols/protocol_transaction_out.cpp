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

#include <functional>
#include <string>
#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

#define NAME "transaction"
#define CLASS protocol_transaction_out

using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_transaction_out::protocol_transaction_out(p2p& network,
    channel::ptr channel)
  : protocol_events(network, channel, NAME),
    relay_to_peer_(peer_version().relay),
    CONSTRUCT_TRACK(protocol_transaction_out)
{
}

void protocol_transaction_out::start()
{
    if (relay_to_peer_)
    {
        // TODO: subscribe to transaction pool notifications and relay txs.
        // TODO: add host id to message subscriber to avoid tx reflection.
    }
}

} // namespace node
} // namespace libbitcoin
