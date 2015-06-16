/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
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

    poller(threadpool& pool, blockchain::blockchain& chain);

    void monitor(bc::network::channel::pointer node);

    void request_blocks(const hash_digest& block_hash,
        bc::network::channel::pointer node);

private:

    ////void receive_inv(const std::error_code& ec,
    ////    const inventory_type& packet, bc::network::channel_ptr node);
    void receive_block(const std::error_code& ec,
        const chain::block& block, bc::network::channel::pointer node);

    void handle_store_block(const std::error_code& ec,
        blockchain::block_info info, const hash_digest& block_hash,
        bc::network::channel::pointer node);

    void ask_blocks(const std::error_code& ec,
        const message::block_locator& locator, const hash_digest& hash_stop,
        bc::network::channel::pointer node);

    bool is_duplicate_block_ask(const message::block_locator& locator,
        const hash_digest& hash_stop, bc::network::channel::pointer node);

    async_strand strand_;
    blockchain::blockchain& blockchain_;

    // Last hash from an inventory packet.
    hash_digest last_block_hash_;

    // Last hash from a block locator.
    hash_digest last_locator_begin_;
    hash_digest last_hash_stop_;
    bc::network::channel* last_requested_node_;
};

} // namespace node
} // namespace libbitcoin

#endif
