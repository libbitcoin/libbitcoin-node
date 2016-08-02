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
#ifndef LIBBITCOIN_NODE_PROTOCOL_BLOCK_HPP
#define LIBBITCOIN_NODE_PROTOCOL_BLOCK_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_block
  : public network::protocol_timer, track<protocol_block>
{
public:
    typedef std::shared_ptr<protocol_block> ptr;

    /// Construct a block protocol instance.
    protocol_block(network::p2p& network, network::channel::ptr channel,
        blockchain::block_chain& blockchain);

    /// Start the protocol.
    virtual void start();

private:
    // Sending.
    //-------------------------------------------------------------------------

    void send_send_headers();
    void send_get_blocks(const code& ec);
    void send_get_data(const code& ec, const hash_list& hashes,
        message::inventory_type_id type_id);

    // responses/notifications
    ////void send_headers(const code& ec);                  // get_headers | announce
    ////void send_inventory(const code& ec);                // get_blocks | announce

    void send_block(const code& ec, chain::block::ptr block,
        const hash_digest& hash);
    void send_merkle_block(const code& ec, message::merkle_block::ptr merkle,
        const hash_digest& hash);

    // Receiving.
    //-------------------------------------------------------------------------

    // requests/control
    bool handle_receive_send_headers(const code& ec, message::send_headers::ptr);
    bool handle_receive_get_headers(const code& ec, message::get_headers::ptr message);
    bool handle_receive_get_blocks(const code& ec, message::get_blocks::ptr message);
    bool handle_receive_get_data(const code& ec, message::get_data::ptr message);

    // responses/notifications
    bool handle_receive_headers(const code& ec, message::headers::ptr message);
    bool handle_receive_inventory(const code& ec, message::inventory::ptr message);
    bool handle_receive_block(const code& ec, message::block::ptr message);
    ////bool handle_receive_merkle_block(const code& ec, message::merkle_block::ptr message);
    ////bool handle_receive_not_found(const code& ec, message::not_found::ptr message);

    // Other.
    //-------------------------------------------------------------------------
    void handle_fetch_missing_orphans(const code& ec, const hash_list& hashes);
    void handle_fetch_block_locator(const code& ec, const hash_list& hashes);
    void handle_fetch_locator_hashes(const code& ec, const hash_list& hashes);
    void handle_fetch_locator_headers(const code& ec,
        const chain::header::list& headers);

    void handle_send(const code& ec, const std::string& command);
    void handle_store_block(const code& ec);
    bool handle_reorganized(const code& ec, size_t fork_point,
        const chain::block::ptr_list& incoming,
        const chain::block::ptr_list& outgoing);

    std::atomic<bool> send_headers_;
    blockchain::block_chain& blockchain_;
};

} // namespace node
} // namespace libbitcoin

#endif
