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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_block_in
  : public node::protocol,
    protected network::tracker<protocol_block_in>
{
public:
    typedef std::shared_ptr<protocol_block_in> ptr;
    using type_id = network::messages::inventory::type_id;

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    template <typename Session>
    protocol_block_in(Session& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_block_in>(session.log),
        block_type_(session.config().network.witness_node() ?
            type_id::witness_block : type_id::block)
    {
    }
    BC_POP_WARNING()

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

protected:
    struct track
    {
        const size_t announced;
        const system::hash_digest last;
        system::hashes hashes;
    };

    typedef std::shared_ptr<track> track_ptr;

    /// Recieved incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::inventory::cptr& message) NOEXCEPT;

    /// Recieved incoming block message.
    virtual bool handle_receive_block(const code& ec,
        const network::messages::block::cptr& message,
        const track_ptr& tracker) NOEXCEPT;

    /// Invoked when initial blocks sync is complete.
    virtual void complete() NOEXCEPT;

    /// Handle organize result.
    virtual void handle_organize(const code& ec, size_t height,
        const system::chain::block::cptr& block_ptr) NOEXCEPT;

private:
    static system::hashes to_hashes(
        const network::messages::get_data& getter) NOEXCEPT;

    network::messages::get_blocks create_get_inventory() const NOEXCEPT;
    network::messages::get_blocks create_get_inventory(
        const system::hash_digest& last) const NOEXCEPT;
    network::messages::get_blocks create_get_inventory(
        system::hashes&& start_hashes) const NOEXCEPT;

    network::messages::get_data create_get_data(
        const network::messages::inventory& message) const NOEXCEPT;

    // Thread safe.
    const network::messages::inventory::type_id block_type_;

    // Protected by strand.
    system::chain::checkpoint top_{};
};

} // namespace node
} // namespace libbitcoin

#endif
