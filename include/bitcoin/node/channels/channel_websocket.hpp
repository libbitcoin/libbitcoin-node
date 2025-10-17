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
#ifndef LIBBITCOIN_NODE_CHANNELS_CHANNEL_WEBSOCKET_HPP
#define LIBBITCOIN_NODE_CHANNELS_CHANNEL_WEBSOCKET_HPP

#include <memory>
#include <bitcoin/node/channels/channel.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base websocket channel state for the node.
class BCN_API channel_websocket
  : public network::channel_websocket,
    public node::channel
{
public:
    typedef std::shared_ptr<node::channel_websocket> ptr;
    using options_t = network::channel_websocket::options_t;

    channel_websocket(const network::logger& log,
        const network::socket::ptr& socket,
        const node::configuration& config, uint64_t identifier=zero,
        const options_t& options={}) NOEXCEPT
      : network::channel_websocket(log, socket, config.network, identifier,
          options),
        node::channel(log, socket, config, identifier)
    {
    }
};

} // namespace node
} // namespace libbitcoin

#endif
