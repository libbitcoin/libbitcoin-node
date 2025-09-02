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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_OBSERVER_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_OBSERVER_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_observer
  : public node::protocol,
    protected network::tracker<protocol_observer>
{
public:
    typedef std::shared_ptr<protocol_observer> ptr;

    // TODO: consider relay may be dynamic (disallowed until current).
    // TODO: current network handshake sets relay based on config only.
    template <typename SessionPtr>
    protocol_observer(const SessionPtr& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        relay_disallowed_
        (
            std::dynamic_pointer_cast<network::channel_peer>(channel)->
                is_negotiated(network::messages::level::bip37) &&
            !session->config().network.enable_relay
        ),
        node_witness_(session->config().network.witness_node()),
        network::tracker<protocol_observer>(session->log)
    {
    }

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /////// Accept incoming get_data message.
    ////virtual bool handle_receive_get_data(const code& ec,
    ////    const network::messages::get_data::cptr& message) NOEXCEPT;

    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::inventory::cptr& message) NOEXCEPT;

    // This is thread safe.
    const bool relay_disallowed_;
    const bool node_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
