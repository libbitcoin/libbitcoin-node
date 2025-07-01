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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_OUT_70012_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_OUT_70012_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_out_31800.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_header_out_70012
  : public protocol_header_out_31800,
    protected network::tracker<protocol_header_out_70012>
{
public:
    typedef std::shared_ptr<protocol_header_out_70012> ptr;

    template <typename SessionPtr>
    protocol_header_out_70012(const SessionPtr& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_header_out_31800(session, channel),
        network::tracker<protocol_header_out_70012>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Process block announcement.
    virtual bool do_announce(header_t link) NOEXCEPT;

    virtual bool handle_receive_send_headers(const code& ec,
        const network::messages::send_headers::cptr& message) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
