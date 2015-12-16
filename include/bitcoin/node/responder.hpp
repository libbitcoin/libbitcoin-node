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
#ifndef LIBBITCOIN_NODE_RESPONDER_HPP
#define LIBBITCOIN_NODE_RESPONDER_HPP

#include <atomic>
#include <cstddef>
#include <system_error>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API responder
{
public:
    responder(chain::blockchain& chain, chain::transaction_pool& tx_pool,
        size_t minimum_start_height);
    void monitor(network::channel_ptr node);

private:
    bool handle_reorg(const std::error_code& ec, uint32_t fork_point,
        const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list&);

    bool receive_get_data(const std::error_code& ec,
        const get_data_type& packet, network::channel_ptr node);
    bool receive_get_blocks(const std::error_code& ec,
        const get_blocks_type& packet, network::channel_ptr node);

    void new_tx_get_data(const get_data_type& packet,
        network::channel_ptr node);
    void handle_tx_exists(const std::error_code& ec, bool exists,
        const hash_digest& hash, network::channel_ptr node);

    void new_block_get_data(const get_data_type& packet,
        network::channel_ptr node);
    void handle_block_exists(const std::error_code& ec,
        const hash_digest& hash, network::channel_ptr node);

    void new_filter_get_data(const get_data_type& packet,
        network::channel_ptr node);

    void send_pool_tx(const std::error_code& ec, const transaction_type& tx,
        const hash_digest& tx_hash, network::channel_ptr node);
    void send_chain_tx(const std::error_code& ec, const transaction_type& tx,
        const hash_digest& tx_hash, network::channel_ptr node);
    void send_tx(const transaction_type& tx, const hash_digest& tx_hash,
        network::channel_ptr node);
    void send_tx_not_found(const hash_digest& tx_hash,
        network::channel_ptr node);

    void send_block(const std::error_code& ec, const block_type& block,
        const hash_digest& block_hash, network::channel_ptr node);
    void send_block_not_found(const hash_digest& block_hash,
        network::channel_ptr node);

    void send_inventory_not_found(inventory_type_id inventory_type,
        const hash_digest& hash, network::channel_ptr node,
        network::channel_proxy::send_handler handler);

    chain::blockchain& blockchain_;
    chain::transaction_pool& tx_pool_;
    std::atomic<uint64_t> last_height_;
    const size_t minimum_start_height_;
};

} // node
} // libbitcoin

#endif

