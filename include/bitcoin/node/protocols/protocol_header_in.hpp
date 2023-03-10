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
#ifndef LIBBITCOIN_NODE_PROTOCOL_HEADER_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_HEADER_IN_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class full_node;

class BCN_API protocol_header_in
  : public node::protocol, network::tracker<protocol_header_in>
{
public:
    typedef std::shared_ptr<protocol_header_in> ptr;

    protocol_header_in(network::session& session,
        const channel_ptr& channel, full_node& node) NOEXCEPT;

    void start() NOEXCEPT override;

private:
    ////////void report(const system::chain::header& header) const;
    ////void send_top_get_headers(const system::hash_digest& stop_hash);
    ////void send_next_get_headers(const system::hash_digest& start_hash);
    ////void handle_fetch_header_locator(const code& ec,
    ////    system::get_headers_ptr message, const system::hash_digest& stop_hash);
    ////
    ////bool handle_receive_headers(const code& ec,
    ////    system::headers_const_ptr message);
    ////void store_header(size_t index,
    ////    system::headers_const_ptr message);
    ////void handle_store_header(const code& ec,
    ////    system::header_const_ptr header, size_t index,
    ////    system::headers_const_ptr message);
    
    void send_send_headers();
    void handle_timeout(const code& ec);
    void handle_stop(const code& ec);
    
    // These are thread safe.
    const bool send_headers_;
    const network::steady_clock::duration header_latency_;
    std::atomic<bool> sending_headers_;
};

} // namespace node
} // namespace libbitcoin

#endif
