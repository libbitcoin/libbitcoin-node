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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_HPP

#include <unordered_set>
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
    template <typename SessionPtr>
    protocol_block_in(const SessionPtr& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_block_in>(session->log),
        block_type_(session->config().network.witness_node() ?
            type_id::witness_block : type_id::block)
    {
    }
    BC_POP_WARNING()

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Squash duplicates and provide constant time retrieval.
    using hashmap = std::unordered_set<system::hash_digest>;

    struct track
    {
        // TODO: optimize, default bucket count is around 8.
        hashmap ids{};
        size_t announced{};
        system::hash_digest last{};
    };

    /// Accept incoming inventory message.
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::inventory::cptr& message) NOEXCEPT;

    /// Accept incoming block message.
    virtual bool handle_receive_block(const code& ec,
        const network::messages::block::cptr& message) NOEXCEPT;
    virtual void handle_organize(const code& ec, size_t height,
        const system::chain::block::cptr& block_ptr) NOEXCEPT;

private:
    static hashmap to_hashes(
        const network::messages::get_data& getter) NOEXCEPT;

    network::messages::get_blocks create_get_inventory() const NOEXCEPT;
    network::messages::get_blocks create_get_inventory(
        const system::hash_digest& last) const NOEXCEPT;
    network::messages::get_blocks create_get_inventory(
        system::hashes&& start_hashes) const NOEXCEPT;

    network::messages::get_data create_get_data(
        const network::messages::inventory& message) const NOEXCEPT;

    // This is protected by strand.
    track tracker_{};

    // This is thread safe.
    const network::messages::inventory::type_id block_type_;
};

} // namespace node
} // namespace libbitcoin

#endif
