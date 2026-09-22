/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_observer
  : public node::protocol_peer,
    protected network::tracker<protocol_observer>
{
public:
    typedef std::shared_ptr<protocol_observer> ptr;

    // TODO: consider relay may be dynamic (disallowed until current).
    // TODO: current network handshake sets relay based on config only.
    protocol_observer(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_peer(session, channel),
        relay_disallowed_
        (
            std::dynamic_pointer_cast<network::channel_peer>(channel)->
                is_negotiated(network::messages::peer::level::bip37) &&
            !session->network_settings().enable_relay
        ),
        group_(to_group<std::decay_t<decltype(*session)>>()),
        network::tracker<protocol_observer>(session->log)
    {
    }

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle chaser events.
    virtual bool handle_chase(const code& ec, event_value value) NOEXCEPT;

    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::peer::inventory::cptr& message) NOEXCEPT;

    /// Add the channel row to a capture of which it is a member.
    virtual bool handle_broadcast_diagnostics(const code& ec,
        const network::diagnostics::cptr& message, uint64_t sender) NOEXCEPT;

    /// Stop the channel if it is the member of a stop.
    virtual bool handle_broadcast_terminator(const code& ec,
        const network::terminator::cptr& message, uint64_t sender) NOEXCEPT;

    /// The capture group of the channel (the session determines the group).
    virtual network::diagnostics::target group() const NOEXCEPT;

private:
    // The session configuration type identifies its capture group.
    template <typename Session>
    static constexpr network::diagnostics::target to_group() NOEXCEPT
    {
        using options = typename Session::options_t;
        using inbound = network::settings::peer_inbound;
        using manual = network::settings::peer_manual;

        if constexpr (is_same_type<options, inbound>)
            return network::diagnostics::target::inbound;
        else if constexpr (is_same_type<options, manual>)
            return network::diagnostics::target::manual;
        else
            return network::diagnostics::target::outbound;
    }

    // These are thread safe.
    const bool relay_disallowed_;
    const network::diagnostics::target group_;
};

} // namespace node
} // namespace libbitcoin

#endif
