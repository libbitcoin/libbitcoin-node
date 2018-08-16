/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOL_BLOCK_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_BLOCK_IN_HPP

#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/hash_queue.hpp>

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
    void start() override;

protected:
    // Expose polymorphic start method from base.
    using network::protocol_timer::start;

private:
    static void report(const chain::block& block, size_t height);

    void send_get_blocks(const hash_digest& stop_hash);
    void send_get_data(const code& ec, get_data_ptr message);

    bool handle_receive_block(const code& ec, block_const_ptr message);
    bool handle_receive_inventory(const code& ec, inventory_const_ptr message);
    bool handle_receive_not_found(const code& ec, not_found_const_ptr message);
    void handle_store_block(const code& ec, size_t height,
        block_const_ptr message);
    void handle_fetch_header_locator(const code& ec, get_blocks_ptr message,
        const hash_digest& stop_hash);

    void handle_timeout(const code& ec);
    void handle_stop(const code& ec);

    // These are thread safe.
    full_node& node_;
    blockchain::safe_chain& chain_;
    hash_queue backlog_;
    const asio::duration block_latency_;
    const bool not_found_;
    const bool blocks_first_;
    const bool blocks_inventory_;
    const bool blocks_from_peer_;
    const bool require_witness_;
    const bool peer_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
