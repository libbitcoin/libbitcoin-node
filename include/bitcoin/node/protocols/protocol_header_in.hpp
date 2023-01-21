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

class BCN_API protocol_header_in
  : public protocol, network::tracker<protocol_header_in>
{
public:
    typedef std::shared_ptr<protocol_header_in> ptr;

    protocol_header_in(auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol(session, channel),
        network::tracker<protocol_header_in>(session.log())
    {
    }

    void start() NOEXCEPT override;

protected:
    const std::string& name() const NOEXCEPT override;

private:
    ////void report(const system::chain::header& header) const;
    ////void send_top_get_headers(const system::hash_digest& stop_hash);
    ////void send_next_get_headers(const system::hash_digest& start_hash);
    ////void handle_fetch_header_locator(const system::code& ec,
    ////    system::get_headers_ptr message, const system::hash_digest& stop_hash);
    ////
    ////bool handle_receive_headers(const system::code& ec,
    ////    system::headers_const_ptr message);
    ////void store_header(size_t index, system::headers_const_ptr message);
    ////void handle_store_header(const system::code& ec,
    ////    system::header_const_ptr header, size_t index,
    ////    system::headers_const_ptr message);
    ////
    ////void send_send_headers();
    ////void handle_timeout(const system::code& ec);
    ////void handle_stop(const system::code& ec);
    ////
    ////// These are thread safe.
    ////full_node& node_;
    ////blockchain::safe_chain& chain_;
    ////const system::asio::duration header_latency_;
    ////const bool send_headers_;
    ////std::atomic<bool> sending_headers_;
};

} // namespace node
} // namespace libbitcoin

#endif
