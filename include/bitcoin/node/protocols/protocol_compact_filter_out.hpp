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
#ifndef LIBBITCOIN_NODE_PROTOCOL_COMPACT_FILTER_OUT_HPP
#define LIBBITCOIN_NODE_PROTOCOL_COMPACT_FILTER_OUT_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

class BCN_API protocol_compact_filter_out
  : public network::protocol_events, track<protocol_compact_filter_out>
{
public:
    typedef std::shared_ptr<protocol_compact_filter_out> ptr;

    /// Construct a compact filter protocol instance.
    protocol_compact_filter_out(full_node& network,
        network::channel::ptr channel, blockchain::safe_chain& chain);

    /// Start the protocol.
    virtual void start();

private:
    bool handle_receive_get_compact_filters(const system::code& ec,
        system::get_compact_filters_const_ptr message);
    bool handle_receive_get_compact_filter_headers(const system::code& ec,
        system::get_compact_filter_headers_const_ptr message);
    bool handle_receive_get_compact_filter_checkpoint(const system::code& ec,
        system::get_compact_filter_checkpoint_const_ptr message);

    void handle_stop(const system::code& ec);

    void handle_compact_filter_headers(const system::code& ec,
        system::compact_filter_headers_ptr message);
    void handle_compact_filter_checkpoint(const system::code& ec,
        system::compact_filter_checkpoint_ptr message);

    void handle_compact_filters_start(
        system::get_compact_filters_const_ptr request, const system::code& ec,
        system::compact_filter_ptr response, size_t height);

    void handle_next_compact_filter(
        system::get_compact_filters_const_ptr request, size_t stop_height,
        const system::code& ec, system::compact_filter_ptr response,
        size_t height);

    // These are thread safe.
    ////full_node& node_;
    blockchain::safe_chain& chain_;
    system::atomic<system::hash_digest> last_locator_top_;
};

} // namespace node
} // namespace libbitcoin

#endif
