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
#ifndef LIBBITCOIN_NODE_PROTOCOL_BLOCK_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_BLOCK_IN_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API protocol_block_in
  : public network::protocol_timer, track<protocol_block_in>
{
public:
    typedef std::shared_ptr<protocol_block_in> ptr;

    /// Construct a block protocol instance.
    protocol_block_in(full_node& network, network::channel::ptr channel,
        blockchain::safe_chain& chain);

    /// Start the protocol.
    virtual void start();

private:
    void get_block_inventory(const code& ec);
    void send_get_blocks(const hash_digest& stop_hash);
    void send_get_data(const code& ec, get_data_ptr message);

    void handle_filter_orphans(const code& ec, get_data_ptr message);
    bool handle_receive_block(const code& ec, block_const_ptr message);
    bool handle_receive_headers(const code& ec, headers_const_ptr message);
    bool handle_receive_inventory(const code& ec, inventory_const_ptr message);
    bool handle_receive_not_found(const code& ec, not_found_const_ptr message);
    void handle_store_block(const code& ec, block_const_ptr message);
    void handle_fetch_block_locator(const code& ec, get_headers_ptr message,
        const hash_digest& stop_hash);

    bool handle_reorganized(code ec, size_t fork_height,
        block_const_ptr_list_const_ptr incoming,
        block_const_ptr_list_const_ptr outgoing);

    full_node& node_;
    blockchain::safe_chain& chain_;
    bc::atomic<hash_digest> last_locator_top_;
    const bool headers_from_peer_;
};

} // namespace node
} // namespace libbitcoin

#endif
