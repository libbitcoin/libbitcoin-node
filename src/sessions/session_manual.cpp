/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/sessions/session_manual.hpp>

#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/protocols/protocol_block_sync.hpp>
#include <bitcoin/node/protocols/protocol_block_out.hpp>
#include <bitcoin/node/protocols/protocol_header_in.hpp>
#include <bitcoin/node/protocols/protocol_transaction_in.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

session_manual::session_manual(full_node& network, safe_chain& chain)
  : session<network::session_manual>(network, true),
    chain_(chain),
    CONSTRUCT_TRACK(node::session_manual)
{
}

void session_manual::attach_protocols(channel::ptr channel)
{
    const auto version = channel->negotiated_version();

    if (version >= version::level::bip31)
        attach<protocol_ping_60001>(channel)->start();
    else
        attach<protocol_ping_31402>(channel)->start();

    if (version >= version::level::bip61)
        attach<protocol_reject_70002>(channel)->start();

    if (version >= version::level::headers)
        attach<protocol_header_in>(channel, chain_)->start();

    attach<protocol_block_sync>(channel, chain_)->start();
    attach<protocol_transaction_in>(channel, chain_)->start();
    attach<protocol_transaction_out>(channel, chain_)->start();
    attach<protocol_address_31402>(channel)->start();
}

} // namespace node
} // namespace libbitcoin
