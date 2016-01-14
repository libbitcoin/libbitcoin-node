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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/poller.hpp>
#include <bitcoin/node/responder.hpp>

namespace libbitcoin {
namespace node {

class BCN_API session
{
public:
    session(threadpool& pool, network::p2p& protocol,
        blockchain::block_chain& blockchain, poller& poller,
        blockchain::transaction_pool& transaction_pool,
        responder& responder, size_t last_checkpoint_height);

    void start();

private:
    void new_channel(const code& ec, network::channel::ptr node);

    bool handle_new_blocks(const code& ec, uint64_t fork_point,
        const blockchain::block_chain::list& new_blocks,
        const blockchain::block_chain::list& replaced_blocks);

    void receive_inv(const code& ec, const message::inventory& packet,
        network::channel::ptr node);
    void receive_get_blocks(const code& ec, const message::get_blocks& packet,
        network::channel::ptr node);
    void new_tx_inventory(const hash_digest& hash, network::channel::ptr node);
    void request_tx_data(const code& ec, const hash_digest& hash,
        network::channel::ptr node);
    void new_block_inventory(const hash_digest& hash,
        network::channel::ptr node);
    void request_block_data(const hash_digest& hash,
        network::channel::ptr node);
    void fetch_block_handler(const code& ec, const chain::block& block,
        const hash_digest hash, network::channel::ptr node);

    dispatcher dispatch_;
    network::p2p& network_;
    blockchain::block_chain& blockchain_;
    blockchain::transaction_pool& tx_pool_;
    node::poller& poller_;
    node::responder& responder_;
    std::atomic<uint64_t> last_height_;
    size_t last_checkpoint_height_;

    // HACK: this is for access to handle_new_blocks to facilitate server
    // inheritance of full_node. The organization should be refactored.
    friend class full_node;
};

} // namespace node
} // namespace libbitcoin

#endif
