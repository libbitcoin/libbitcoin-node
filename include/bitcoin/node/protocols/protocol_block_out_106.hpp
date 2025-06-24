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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_106_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_106_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_block_out_106
  : public node::protocol,
    protected network::tracker<protocol_block_out_106>
{
public:
    typedef std::shared_ptr<protocol_block_out_106> ptr;

    template <typename SessionPtr>
    protocol_block_out_106(const SessionPtr& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        block_type_(session->config().network.witness_node() ?
            type_id::witness_block : type_id::block),
        network::tracker<protocol_block_out_106>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    virtual bool disabled() const NOEXCEPT;

    virtual bool handle_receive_get_blocks(const code& ec,
        const network::messages::get_blocks::cptr& message) NOEXCEPT;
    virtual bool handle_receive_get_data(const code& ec,
        const network::messages::get_data::cptr& message) NOEXCEPT;
    virtual void send_block(const code& ec, size_t index,
        const network::messages::get_data::cptr& message) NOEXCEPT;
    virtual bool handle_broadcast_block(const code& ec,
        const network::messages::block::cptr& message,
        uint64_t sender) NOEXCEPT;

private:
    using type_id = network::messages::inventory_item::type_id;

    network::messages::inventory create_inventory(
        const network::messages::get_blocks& locator) const NOEXCEPT;

    // This is thread safe.
    const network::messages::inventory::type_id block_type_;

    // This is protected by strand.
    bool disabled_{};
};

} // namespace node
} // namespace libbitcoin

#endif
