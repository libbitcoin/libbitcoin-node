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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

// Individual inclusion to prevent cycle (can't forward declare).
#include <bitcoin/node/sessions/session.hpp>
////class session;

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols.
class BCN_API protocol
  : public network::protocol
{
public:
    DELETE_COPY_MOVE(protocol);

protected:
    typedef network::channel::ptr channel_ptr;

    template <typename Session>
    protocol(Session& session, const channel_ptr& channel) NOEXCEPT
      : network::protocol(session, channel), session_(session)
    {
    }

    virtual ~protocol() NOEXCEPT;

    /// Report performance, handler may direct self-terminate.
    virtual void performance(uint64_t channel, uint64_t speed,
        network::result_handler&& handler) const NOEXCEPT;

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        network::result_handler&& handler) NOEXCEPT;

    /// Organize a validated block.
    virtual void organize(const system::chain::block::cptr& block,
        network::result_handler&& handler) NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    full_node::query& archive() const NOEXCEPT;

private:
    session& session_;
};

} // namespace node
} // namespace libbitcoin

#endif
