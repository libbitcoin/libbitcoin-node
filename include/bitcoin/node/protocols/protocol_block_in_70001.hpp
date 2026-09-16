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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_70001_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_70001_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_block_in_106.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_block_in_70001
  : public protocol_block_in_106,
    protected network::tracker<protocol_block_in_70001>
{
public:
    typedef std::shared_ptr<protocol_block_in_70001> ptr;

    protocol_block_in_70001(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_block_in_106(session, channel),
        network::tracker<protocol_block_in_70001>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Accept incoming not_found message.
    virtual bool handle_receive_not_found(const code& ec,
        const network::messages::peer::not_found::cptr& message) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
