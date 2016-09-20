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
#ifndef LIBBITCOIN_NODE_PROTOCOL_BLOCK_OUT_HPP
#define LIBBITCOIN_NODE_PROTOCOL_BLOCK_OUT_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_block_out
  : public network::protocol_events, track<protocol_block_out>
{
public:
    typedef std::shared_ptr<protocol_block_out> ptr;

    /// Construct a block protocol instance.
    protocol_block_out(network::p2p& network, network::channel::ptr channel,
        blockchain::full_chain& blockchain);

    /// Start the protocol.
    virtual void start();

private:
    void send_block(const code& ec, block_ptr message, uint64_t height,
        const hash_digest& hash);
    void send_merkle_block(const code& ec, merkle_block_const_ptr message,
        const hash_digest& hash);

    bool handle_receive_get_data(const code& ec, get_data_const_ptr message);
    bool handle_receive_get_blocks(const code& ec,
        get_blocks_const_ptr message);
    bool handle_receive_get_headers(const code& ec,
        get_headers_const_ptr message);
    bool handle_receive_send_headers(const code& ec,
        send_headers_const_ptr message);

    void handle_fetch_locator_hashes(const code& ec, inventory_ptr message);
    void handle_fetch_locator_headers(const code& ec, headers_ptr message);

    void handle_stop(const code&);
    bool handle_reorganized(const code& ec, size_t fork_height,
        const block_const_ptr_list& incoming,
        const block_const_ptr_list& outgoing);

    blockchain::full_chain& blockchain_;
    bc::atomic<hash_digest> last_locator_top_;
    std::atomic<size_t> current_chain_height_;
    std::atomic<bool> headers_to_peer_;
};

} // namespace node
} // namespace libbitcoin

#endif
