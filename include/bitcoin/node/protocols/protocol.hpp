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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HPP

#include <memory>
#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

// Only session.hpp.
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols, thread safe.
class BCN_API protocol
{
protected:
    DELETE_COPY_MOVE_DESTRUCT(protocol);

    /// Constructors.
    /// -----------------------------------------------------------------------

    // reinterpret_pointer_cast because channel is abstract.
    inline protocol(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : channel_(std::reinterpret_pointer_cast<node::channel>(channel)),
        session_(session)
    {
    }

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// The candidate|confirmed chain is current.
    virtual bool is_current(bool confirmed) const NOEXCEPT;

private:
    // This channel requires stranded calls, base is thread safe.
    const node::channel::ptr channel_;

    // This is thread safe.
    const node::session::ptr session_;
};

} // namespace node
} // namespace libbitcoin

#endif
