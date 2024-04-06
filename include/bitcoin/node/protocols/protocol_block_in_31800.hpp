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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_31800_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_31800_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_performer.hpp>

namespace libbitcoin {
namespace node {
    
/// This does NOT inherit from protocol_block_in.
class BCN_API protocol_block_in_31800
  : public protocol_performer
{
public:
    typedef std::shared_ptr<protocol_block_in_31800> ptr;

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    template <typename Session>
    protocol_block_in_31800(Session& session,
        const channel_ptr& channel, bool performance) NOEXCEPT
      : protocol_performer(session, channel, performance),
        block_type_(session.config().network.witness_node() ?
            type_id::witness_block : type_id::block),
        map_(std::make_shared<database::associations>())
    {
    }
    BC_POP_WARNING()

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Determine if block passes check validation.
    virtual code check(const system::chain::block& block,
        const system::chain::context& ctx) const NOEXCEPT;

    /// Get published download identifiers.
    virtual void handle_event(const code& ec,
        chase event_, event_link value) NOEXCEPT;
    virtual void do_get_downloads(count_t count) NOEXCEPT;

    /// Manage work splitting.
    bool is_idle() const NOEXCEPT override;
    virtual void do_purge(channel_t) NOEXCEPT;
    virtual void do_split(channel_t) NOEXCEPT;
    virtual void do_pause(channel_t) NOEXCEPT;
    virtual void do_resume(channel_t) NOEXCEPT;

    /// Check incoming block message.
    virtual bool handle_receive_block(const code& ec,
        const network::messages::block::cptr& message) NOEXCEPT;

private:
    using type_id = network::messages::inventory::type_id;
    static constexpr size_t minimum_for_stall_divide = 2;

    void send_get_data(const map_ptr& map) NOEXCEPT;
    network::messages::get_data create_get_data(
        const map_ptr& map) const NOEXCEPT;

    map_ptr split(const map_ptr& map) NOEXCEPT;
    void restore(const map_ptr& map) NOEXCEPT;
    void handle_put_hashes(const code& ec) NOEXCEPT;
    void handle_get_hashes(const code& ec, const map_ptr& map) NOEXCEPT;

    // This is thread safe.
    const network::messages::inventory::type_id block_type_;

    //  This is protected by strand.
    map_ptr map_;
};

} // namespace node
} // namespace libbitcoin

#endif
