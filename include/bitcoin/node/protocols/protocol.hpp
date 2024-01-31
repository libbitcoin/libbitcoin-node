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
#include <bitcoin/node/protocols/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols.
class BCN_API protocol
  : public network::protocol
{
protected:
    typedef network::channel::ptr channel_ptr;

    template <typename Session>
    protocol(Session& session, const channel_ptr& channel) NOEXCEPT
      : network::protocol(session, channel), session_(session)
    {
    }

    /// Report performance, false directs self-terminate.
    void performance(uint64_t channel, size_t speed,
        network::result_handler&& handler) const NOEXCEPT;

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
