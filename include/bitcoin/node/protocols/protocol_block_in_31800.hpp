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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_31800_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_IN_31800_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_performer.hpp>

namespace libbitcoin {
namespace node {
    
/// This class does NOT inherit from protocol_block_in_106.
class BCN_API protocol_block_in_31800
  : public protocol_performer,
    protected network::tracker<protocol_block_in_31800>
{
public:
    typedef std::shared_ptr<protocol_block_in_31800> ptr;

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    template <typename SessionPtr>
    protocol_block_in_31800(const SessionPtr& session,
        const network::channel::ptr& channel, bool performance_enabled=true) NOEXCEPT
      : protocol_performer(session, channel, performance_enabled),
        top_checkpoint_height_(
            session->config().bitcoin.top_checkpoint().height()),
        block_type_(session->config().network.witness_node() ?
            type_id::witness_block : type_id::block),
        map_(chaser_check::empty_map()),
        network::tracker<protocol_block_in_31800>(session->log)
    {
    }
    BC_POP_WARNING()

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle event subscription completion.
    void subscribed(const code& ec, object_key key) NOEXCEPT override;

    /// Get published download identifiers.
    virtual void do_get_downloads(count_t count) NOEXCEPT;

    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// Manage work splitting.
    bool is_idle() const NOEXCEPT override;
    virtual void do_purge(channel_t) NOEXCEPT;
    virtual void do_split(channel_t) NOEXCEPT;
    virtual void do_report(count_t count) NOEXCEPT;

    /// Check incoming block message.
    virtual bool handle_receive_block(const code& ec,
        const network::messages::p2p::block::cptr& message) NOEXCEPT;

private:
    code check(const system::chain::block& block,
        const system::chain::context& ctx, bool bypass) const NOEXCEPT;

    void send_get_data(const map_ptr& map, const job::ptr& job) NOEXCEPT;
    network::messages::p2p::get_data create_get_data(
        const database::associations& map) const NOEXCEPT;

    void restore(const map_ptr& map) NOEXCEPT;
    bool is_under_checkpoint(size_t height) const NOEXCEPT;
    void handle_put_hashes(const code& ec, size_t count) NOEXCEPT;
    void handle_get_hashes(const code& ec, const map_ptr& map,
        const job::ptr& job) NOEXCEPT;

    // These are thread safe.
    const size_t top_checkpoint_height_;
    const type_id block_type_;

    // These are protected by strand.
    map_ptr map_;
    job::ptr job_{};

    std_vector<system::chain::block::cptr> blocks_{};
};

} // namespace node
} // namespace libbitcoin

#endif
