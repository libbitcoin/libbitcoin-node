/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_P2P_NODE_HPP
#define LIBBITCOIN_NODE_P2P_NODE_HPP

#include <cstdint>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// A full node on the Bitcoin P2P network.
class BCN_API p2p_node
  : public network::p2p
{
public:
    static const configuration defaults;

    /// Construct the full node.
    p2p_node(const configuration& configuration);

    /// Synchronize the blockchain and then start the long-running sessions.
    virtual void run(result_handler handler);

private:
    void handle_synchronized(const code& ec, result_handler handler);

    configuration configuration_;
};

} // namspace node
} //namespace libbitcoin

#endif
