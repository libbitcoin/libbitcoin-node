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
    static std::string to_text(inventory_type_id type);
    static size_t count(const inventory_list& inventories,
        inventory_type_id type_id);
    static hash_list to_hashes(const inventory_list& inventories,
        inventory_type_id type);
    static inventory_list to_inventories(const hash_list& hashes,
        inventory_type_id type);
    static inventory_list to_inventories(
        const chain::blockchain::block_list& blocks);

    inventory(network::handshake& handshake, chain::blockchain& chain,
        chain::transaction_pool& tx_pool, size_t minimum_start_height);
    void monitor(network::channel_ptr node);
    void set_start_height(uint64_t height);

private:
    bool handle_reorg(const std::error_code& ec, uint32_t fork_point,
        const chain::blockchain::block_list& new_blocks,
        const chain::blockchain::block_list&);
    void handle_set_height(const std::error_code& ec, uint32_t height);

    bool receive_inv(const std::error_code& ec,
        const inventory_type& packet, network::channel_ptr node);

    void new_transaction_inventory(const inventory_type& packet,
        network::channel_ptr node);
    void get_transactions(const std::error_code& ec, const hash_list& hashes,
        network::channel_ptr node);

    void new_block_inventory(const inventory_type& packet,
        network::channel_ptr node);
    void get_blocks(const std::error_code& ec, const hash_list& hashes,
        network::channel_ptr node);

    void new_filter_inventory(const inventory_type& packet,
        network::channel_ptr node);

    network::handshake& handshake_;
    chain::blockchain& blockchain_;
    chain::transaction_pool& tx_pool_;
    std::atomic<uint64_t> last_height_;
    const size_t minimum_start_height_;
};

} // namespace node
} // namespace libbitcoin

#endif

