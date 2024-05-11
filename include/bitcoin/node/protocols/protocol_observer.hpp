/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

    template <typename SessionPtr>
    protocol_observer(const SessionPtr& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_observer>(session->log)
    {
    }

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle event subscription completion.
    virtual void complete_event(const code& ec, object_key key) NOEXCEPT;

    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

private:
    void do_complete_event(const code& e) NOEXCEPT;

    // This is protected by strand.
    object_key key_{};
};

} // namespace node
} // namespace libbitcoin

#endif
