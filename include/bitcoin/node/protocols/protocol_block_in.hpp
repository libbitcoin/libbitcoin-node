/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_protocol_block_in_HPP
#define LIBBITCOIN_NODE_protocol_block_in_HPP

#include <cstddef>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/reservation.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Blocks sync protocol, thread safe.
class BCN_API protocol_block_in
  : public network::protocol_timer, public track<protocol_block_in>
{
public:
    typedef std::shared_ptr<protocol_block_in> ptr;

    /// Construct a block sync protocol instance.
    protocol_block_in(full_node& node, network::channel::ptr channel,
        blockchain::safe_chain& chain);

    /// Start the protocol.
    void start() override;

protected:
    // Expose polymorphic start method from base.
    using network::protocol_timer::start;

private:
    void report(const system::chain::block& block, size_t height) const;
    void send_get_blocks();
    void handle_event(const system::code& ec);
    bool handle_receive_block(const system::code& ec,
        system::block_const_ptr message);
    bool handle_reindexed(system::code ec, size_t fork_height,
        system::header_const_ptr_list_const_ptr incoming,
        system::header_const_ptr_list_const_ptr outgoing);

    // These are thread safe.
    full_node& node_;
    blockchain::safe_chain& chain_;
    reservation::ptr reservation_;
    const bool require_witness_;
    const bool peer_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
