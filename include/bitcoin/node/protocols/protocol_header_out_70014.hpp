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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_OUT_70014_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_OUT_70014_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_out_70012.hpp>

namespace libbitcoin {
namespace node {

/// Announce blocks as compact blocks (bip152 version 2) when requested.
class BCN_API protocol_header_out_70014
  : public protocol_header_out_70012,
    protected network::tracker<protocol_header_out_70014>
{
public:
    typedef std::shared_ptr<protocol_header_out_70014> ptr;

    protocol_header_out_70014(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_header_out_70012(session, channel),
        network::tracker<protocol_header_out_70014>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Process block announcement.
    bool do_announce(header_t link) NOEXCEPT override;

    virtual bool handle_receive_send_compact(const code& ec,
        const network::messages::peer::send_compact::cptr& message) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
