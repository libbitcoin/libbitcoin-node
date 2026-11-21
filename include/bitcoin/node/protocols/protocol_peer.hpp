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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_PEER_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_PEER_HPP

#include <bitcoin/node/channels/channels.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

 // Only session.hpp.
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base for node peer protocols, thread safe.
class BCN_API protocol_peer
  : public node::protocol,
    public network::protocol_peer
{
public:
    // Replace base class channel_t (network::channel_peer). 
    using channel_t = node::channel_peer;
    using gate_t = network::channel_peer::gate_t;

protected:
    /// Constructors.
    /// -----------------------------------------------------------------------
    /// static_pointer_cast relies on create_channel().

    inline protocol_peer(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::protocol_peer(session, channel),
        channel_(std::static_pointer_cast<node::channel_peer>(channel)),
        session_(session)
    {
    }

    /// Organizers.
    /// -----------------------------------------------------------------------

    /// Get block hashes for blocks to download.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;

    /// Submit block hashes for blocks not downloaded.
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Get the block of a compact block that was not filled, as work.
    virtual void get_unfilled(header_t link, map_handler&& handler) NOEXCEPT;

    /// Set the channel's handler of work offered by its own protocols.
    virtual void set_downloader(channel_peer::downloader&& handler) NOEXCEPT;

    /// Offer work to the channel's downloader, false if there is none.
    virtual bool download(const map_ptr& map, const job::ptr& job) NOEXCEPT;

    /// Reserve or release a high bandwidth compact peer slot.
    virtual bool reserve_compact() NOEXCEPT;
    virtual void release_compact() NOEXCEPT;

    /// The memory pool of the peer is requested once relaying.
    virtual bool request_mempool() const NOEXCEPT;

    /// Reserve or release the channel's relay slot, false if none is free.
    virtual bool reserve_relay() NOEXCEPT;
    virtual void release_relay() NOEXCEPT;

    /// The channel's relay slot, relay_slots if none.
    virtual size_t relay_slot() const NOEXCEPT;

    /// Set the fee rate of the channel's peer for relay.
    virtual void set_relay_rate(uint64_t rate) NOEXCEPT;

    /// Mark the pooled txs as held by the channel's peer.
    virtual void set_relayed(const database::tx_links& links) NOEXCEPT;

    /// Relay to the channel's peer the pooled txs it lacks at its rate.
    virtual void relay_pool() NOEXCEPT;

    /// Methods.
    /// -----------------------------------------------------------------------

    /// Report performance, handler may direct self-terminate.
    virtual void performance(uint64_t speed,
        network::result_handler&& handler) const NOEXCEPT;

    /// Suspend all existing and future network connections.
    /// A race condition could result in an unsuspended connection.
    virtual code fault(const code& ec) NOEXCEPT;

    /// Announcements.
    /// -----------------------------------------------------------------------

    /// Set an incoming block or tx hash that peer announced.
    virtual void set_announced(const system::hash_digest&) NOEXCEPT;

    /// Determine if outgoing block or tx was previously announced by peer.
    virtual bool was_announced(const system::hash_digest&) const NOEXCEPT;

    /// Currency.
    /// -----------------------------------------------------------------------

    /// Set chain currency, which is not latched (channel buffer policy).
    virtual void set_current(bool value) NOEXCEPT;

    /// Events notification.
    /// -----------------------------------------------------------------------

    using network::protocol_peer::notify;
    using node::protocol::notify;

    /// Compact blocks (bip152 version 2).
    /// -----------------------------------------------------------------------

    /// Compact block of an associated block (coinbase prefilled), or null.
    virtual network::messages::peer::compact_block::cptr make_compact_block(
        const database::header_link& link) const NOEXCEPT;

private:
    // This derived channel requires stranded calls, base is thread safe.
    const node::channel_peer::ptr channel_;

    // This is thread safe.
    const node::session::ptr session_;
};

} // namespace node
} // namespace libbitcoin

#endif
