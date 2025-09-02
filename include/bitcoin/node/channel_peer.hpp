/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHANNEL_PEER_HPP
#define LIBBITCOIN_NODE_CHANNEL_PEER_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Peer channel state for the node.
class BCN_API channel_peer
  : public network::channel_peer
{
public:
    typedef std::shared_ptr<node::channel_peer> ptr;

    channel_peer(network::memory& memory, const network::logger& log,
        const network::socket::ptr& socket, const node::configuration& config,
        uint64_t identifier=zero) NOEXCEPT;

    void set_announced(const system::hash_digest& hash) NOEXCEPT;
    bool was_announced(const system::hash_digest& hash) const NOEXCEPT;

private:
    // This is protected by strand.
    boost::circular_buffer<system::hash_digest> announced_;
};

} // namespace node
} // namespace libbitcoin

#endif
