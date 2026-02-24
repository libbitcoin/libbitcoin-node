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
#ifndef LIBBITCOIN_NODE_PROTOCOL_PERFORMER_HPP
#define LIBBITCOIN_NODE_PROTOCOL_PERFORMER_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {

/// Abstract base protocol for performance standard deviation measurement.
class BCN_API protocol_performer
  : public node::protocol_peer,
    protected network::tracker<protocol_performer>
{
public:
    virtual void start_performance() NOEXCEPT;
    virtual void pause_performance() NOEXCEPT;
    virtual void stop_performance() NOEXCEPT;
    virtual void count(size_t bytes) NOEXCEPT;

protected:
    protocol_performer(const auto& session,
        const network::channel::ptr& channel, bool enabled) NOEXCEPT
      : node::protocol_peer(session, channel),
        deviation_(session->node_settings().allowed_deviation > 0.0),
        enabled_(enabled && to_bool(session->node_settings().sample_period_seconds)),
        performance_timer_(std::make_shared<network::deadline>(session->log,
            channel->strand(), session->node_settings().sample_period())),
        network::tracker<protocol_performer>(session->log)
    {
    }

    virtual bool is_idle() const NOEXCEPT = 0;

private:
    void handle_performance_timer(const code& ec) NOEXCEPT;
    void handle_send_performance(const code& ec) NOEXCEPT;
    void do_handle_performance(const code& ec) NOEXCEPT;

    void send_performance(uint64_t rate) NOEXCEPT;

    // These are thread safe.
    const bool deviation_;
    const bool enabled_;

    // These are protected by strand.
    uint64_t bytes_{ zero };
    network::steady_clock::time_point start_{};
    network::deadline::ptr performance_timer_;
};

} // namespace node
} // namespace libbitcoin

#endif
