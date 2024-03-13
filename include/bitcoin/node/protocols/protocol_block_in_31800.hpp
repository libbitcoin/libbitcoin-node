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

#include <functional>
#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
/// This does NOT inhereit from protocol_block_in.
class BCN_API protocol_block_in_31800
  : public node::protocol,
    protected network::tracker<protocol_block_in_31800>
{
public:
    typedef std::shared_ptr<protocol_block_in_31800> ptr;
    using type_id = network::messages::inventory::type_id;
    using map_ptr = chaser_check::map_ptr;

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    template <typename Session>
    protocol_block_in_31800(Session& session,
        const channel_ptr& channel, bool) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_block_in_31800>(session.log),
        drop_stalled_(to_bool(session.config().node.sample_period_seconds)),
        use_deviation_(session.config().node.allowed_deviation > 0.0),
        block_type_(session.config().network.witness_node() ?
            type_id::witness_block : type_id::block),
        performance_timer_(std::make_shared<network::deadline>(session.log,
            channel->strand(), session.config().node.sample_period())),
        map_(std::make_shared<database::associations>())
    {
    }
    BC_POP_WARNING()

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Performance polling.
    virtual void start_performance() NOEXCEPT;
    virtual void pause_performance() NOEXCEPT;
    virtual void stop_performance() NOEXCEPT;

    /// Get published download identifiers.
    virtual void handle_event(const code& ec,
        chaser::chase event_, chaser::link value) NOEXCEPT;
    virtual void do_get_downloads(chaser::count_t count) NOEXCEPT;
    virtual void do_split(chaser::channel_t channel) NOEXCEPT;

    /// Accept incoming block message.
    virtual bool handle_receive_block(const code& ec,
        const network::messages::block::cptr& message) NOEXCEPT;

private:
    void handle_performance_timer(const code& ec) NOEXCEPT;
    void handle_send_performance(const code& ec) NOEXCEPT;
    void do_handle_performance(const code& ec) NOEXCEPT;

    void send_get_data(const map_ptr& map) NOEXCEPT;
    network::messages::get_data create_get_data(
        const map_ptr& map) const NOEXCEPT;

    map_ptr split(const map_ptr& map) NOEXCEPT;
    void restore(const map_ptr& map) NOEXCEPT;
    void handle_put_hashes(const code& ec) NOEXCEPT;
    void handle_get_hashes(const code& ec, const map_ptr& map) NOEXCEPT;

    void send_performance(uint64_t rate) NOEXCEPT;

    // These are thread safe.
    const bool drop_stalled_;
    const bool use_deviation_;
    const network::messages::inventory::type_id block_type_;

    // These are protected by strand.
    uint64_t bytes_{ zero };
    network::steady_clock::time_point start_{};
    network::deadline::ptr performance_timer_;
    map_ptr map_;
};

} // namespace node
} // namespace libbitcoin

#endif
