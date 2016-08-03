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
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP

#include <memory>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_out
  : public network::protocol_events, track<protocol_transaction_out>
{
public:
    typedef std::shared_ptr<protocol_transaction_out> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_out(network::p2p& network, network::channel::ptr channel);

    /// Start the protocol.
    virtual void start();

private:
    ////void send_inventory(const code& ec);
    ////void send_transaction(const hash_digest& hash);
    ////bool handle_receive_get_data(const code& ec, message::get_data::ptr message);
    ////void handle_memory_pool(const code& ec, ...);

    // Relay unconfirmed transactions to the peer.
    const bool relay_to_peer_;
};

} // namespace node
} // namespace libbitcoin

#endif
