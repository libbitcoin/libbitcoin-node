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
#ifndef LIBBITCOIN_NODE_PROTOCOL_HEADER_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_HEADER_IN_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API protocol_header_in
  : public network::protocol_timer, track<protocol_header_in>
{
public:
    typedef std::shared_ptr<protocol_header_in> ptr;

    /// Construct a block protocol instance.
    protocol_header_in(full_node& network, network::channel::ptr channel,
        blockchain::safe_chain& chain);

    /// Start the protocol.
    void start() override;

protected:
    // Expose polymorphic start method from base.
    using network::protocol_timer::start;

private:
    void send_top_get_headers(const hash_digest& stop_hash);
    void send_next_get_headers(const hash_digest& start_hash);
    void handle_fetch_header_locator(const code& ec, get_headers_ptr message,
        const hash_digest& stop_hash);

    bool handle_receive_headers(const code& ec, headers_const_ptr message);
    void store_header(size_t index, headers_const_ptr message);
    void handle_store_header(const code& ec, size_t index,
        headers_const_ptr message);

    void send_send_headers();
    void handle_timeout(const code& ec);
    void handle_stop(const code& ec);

    // These are thread safe.
    full_node& node_;
    blockchain::safe_chain& chain_;
    const asio::duration header_latency_;
    const bool send_headers_;
    std::atomic<bool> sending_headers_;
};

} // namespace node
} // namespace libbitcoin

#endif
