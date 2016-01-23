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
#ifndef LIBBITCOIN_NODE_SESSION_HPP
#define LIBBITCOIN_NODE_SESSION_HPP

#include <cstddef>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/inventory.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>

namespace libbitcoin {
namespace node {

class BCN_API session
{
public:
    typedef std::function<void (const std::error_code&)> completion_handler;

    session(network::protocol& protocol, chain::blockchain& blockchain,
        chain::transaction_pool& tx_pool, poller& poller, responder& responder,
        inventory& inventory, size_t minimum_start_height=0);

    void start(completion_handler handle_complete);
    void stop(completion_handler handle_complete);

    void broadcast(const chain::blockchain::block_list& blocks);

private:
    bool handle_reorg(const std::error_code& ec, uint64_t fork_point,
        const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list&);

    void handle_started(const std::error_code& ec,
        completion_handler handle_complete);
    bool handle_new_channel(const std::error_code& ec,
        network::channel_ptr node);

    network::protocol& protocol_;
    chain::blockchain& blockchain_;
    chain::transaction_pool& tx_pool_;
    node::poller& poller_;
    node::responder& responder_;
    node::inventory& inventory_;
    const size_t minimum_start_height_;
};

} // namespace node
} // namespace libbitcoin

#endif

