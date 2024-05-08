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

 // Individual session.hpp inclusion to prevent cycle (can't forward declare).
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node protocols, thread safe.
class BCN_API protocol
  : public network::protocol
{
protected:
    typedef network::channel::ptr channel_ptr;

    /// Constructors.
    /// -----------------------------------------------------------------------

    // Need template (see session member)?
    template <typename SessionPtr>
    protocol(const SessionPtr& session, const channel_ptr& channel) NOEXCEPT
      : network::protocol(session, channel), session_(session)
    {
    }

    virtual ~protocol() NOEXCEPT;

    /// Organizers.
    /// -----------------------------------------------------------------------

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        organize_handler&& handler) NOEXCEPT;

    /// Organize a checked block.
    virtual void organize(const system::chain::block::cptr& block,
        organize_handler&& handler) NOEXCEPT;

    /// Get block hashes for blocks to download.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;

    /// Submit block hashes for blocks not downloaded.
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Report performance, handler may direct self-terminate.
    virtual void performance(uint64_t channel, uint64_t speed,
        network::result_handler&& handler) const NOEXCEPT;

    /// Suspend all connections.
    virtual void suspend(const code& ec) NOEXCEPT;

    /// Events.
    /// -----------------------------------------------------------------------

    /// Subscribe to chaser events.
    virtual void subscribe_events(event_handler&& handler,
        event_completer&& complete) NOEXCEPT;

    /// Set a chaser event.
    virtual void notify(const code& ec, chase event_,
        event_link value) NOEXCEPT;

    /// Unsubscribe from chaser events.
    virtual void unsubscribe_events(object_key key) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// The candidate chain is current.
    virtual bool is_current() const NOEXCEPT;

private:
    // This is thread safe.
    const session::ptr session_;

    // This is protected by strand.
    object_key key_{};
};

} // namespace node
} // namespace libbitcoin

#endif
