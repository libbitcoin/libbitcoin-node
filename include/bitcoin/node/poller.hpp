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
#ifndef LIBBITCOIN_NODE_POLLER_HPP
#define LIBBITCOIN_NODE_POLLER_HPP

#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API poller
{
public:
    poller(threadpool& pool, blockchain::block_chain& chain);

    void monitor(network::channel::ptr node);
    void request_blocks(const hash_digest& block_hash,
        network::channel::ptr node);

private:

    ////void receive_inv(const code& ec, const inventory_type& packet,
    ////    network::channel::ptr node);

    void receive_block(const code& ec, const message::block& block,
        network::channel::ptr node);
    void handle_store_block(const code& ec, const blockchain::block_info& info,
        const hash_digest& hash, network::channel::ptr node);

    void get_blocks(const code& ec, const message::block_locator& locator,
        const hash_digest& stop, network::channel::ptr node);
    void handle_get_blocks(const code& ec, network::channel::ptr node,
        const hash_digest& start, const hash_digest& stop);

    dispatcher dispatch_;
    blockchain::block_chain& blockchain_;
};

} // namespace node
} // namespace libbitcoin

#endif
