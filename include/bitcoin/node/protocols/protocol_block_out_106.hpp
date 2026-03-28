/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_106_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_106_HPP

#include <deque>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_block_out_106
  : public node::protocol_peer,
    protected network::tracker<protocol_block_out_106>
{
public:
    typedef std::shared_ptr<protocol_block_out_106> ptr;

    protocol_block_out_106(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_peer(session, channel),
        node_witness_(session->network_settings().witness_node()),
        allow_overlapped_(session->node_settings().allow_overlapped),
        network::tracker<protocol_block_out_106>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using get_data = network::messages::peer::get_data;
    using get_blocks = network::messages::peer::get_blocks;

    /// Block announcements are superseded by send_headers.
    virtual bool superseded() const NOEXCEPT;

    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Process block announcement.
    virtual bool do_announce(header_t link) NOEXCEPT;

    virtual bool handle_receive_get_blocks(const code& ec,
        const get_blocks::cptr& message) NOEXCEPT;
    virtual bool handle_receive_get_data(const code& ec,
        const get_data::cptr& message) NOEXCEPT;
    virtual void send_block(const code& ec) NOEXCEPT;

private:
    using inventory = network::messages::peer::inventory;
    using inventory_item = network::messages::peer::inventory_item;
    using inventory_items = network::messages::peer::inventory_items;

    inventory create_inventory(const get_blocks& locator) const NOEXCEPT;
    void merge_inventory(const inventory_items& items) NOEXCEPT;

    // These are thread safe.
    const bool node_witness_;
    const bool allow_overlapped_;

    // This is protected by strand.
    std::deque<inventory_item> backlog_{};
};

} // namespace node
} // namespace libbitcoin

#endif
