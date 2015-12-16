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
#ifndef LIBBITCOIN_NODE_INVENTORY_HPP
#define LIBBITCOIN_NODE_INVENTORY_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

class BCN_API inventory
{
public:
    inventory(network::handshake& handshake, chain::blockchain& chain,
        chain::transaction_pool& tx_pool, size_t minimum_start_height);
    void monitor(network::channel_ptr node);

private:
    bool set_height(const std::error_code& ec, uint32_t fork_point,
        const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list& /* replaced_blocks */);
    void handle_set_height(const std::error_code& ec, uint32_t height);

    bool receive_inv(const std::error_code& ec,
        const inventory_type& packet, network::channel_ptr node);

    void new_tx_inventory(const inventory_type& packet,
        network::channel_ptr node);
    void get_tx(const std::error_code& ec, bool exists,
        const hash_digest& hash, network::channel_ptr node);

    void new_block_inventory(const inventory_type& packet,
        network::channel_ptr node);
    void get_block(const std::error_code& ec, const hash_digest& hash,
        network::channel_ptr node);

    void new_filter_inventory(const inventory_type& packet,
        network::channel_ptr node);

    network::handshake& handshake_;
    chain::blockchain& blockchain_;
    chain::transaction_pool& tx_pool_;
    std::atomic<uint64_t> last_height_;
    hash_digest last_block_hash_;
    size_t minimum_start_height_;
};

} // namespace node
} // namespace libbitcoin

#endif

