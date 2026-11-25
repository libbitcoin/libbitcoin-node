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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70013_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70013_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out_70001.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_out_70013
  : public protocol_transaction_out_70001,
    protected network::tracker<protocol_transaction_out_70013>
{
public:
    typedef std::shared_ptr<protocol_transaction_out_70013> ptr;

    protocol_transaction_out_70013(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_transaction_out_70001(session, channel),
        network::tracker<protocol_transaction_out_70013>(session->log),
        fee_timer_(to_bool(session->node_settings().fee_filter_minutes) ?
            system::emplace_shared<network::deadline>(session->log,
                channel->strand(),
                session->node_settings().fee_filter_period()) : nullptr)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle chaser events.
    bool handle_chase(const code& ec, event_value value) NOEXCEPT override;
    void subscribed(const code& ec, object_key key) NOEXCEPT override;

    /// Capture the peer's minimum fee rate for announcements, a peer that
    /// lowers it again within the fee filter period is dropped.
    virtual bool handle_receive_fee_filter(const code& ec,
        const network::messages::peer::fee_filter::cptr& message) NOEXCEPT;

private:
    void do_send_fee_filter() NOEXCEPT;
    void handle_fee_timer(const code& ec) NOEXCEPT;

    // This is thread safe.
    const network::deadline::ptr fee_timer_;

    // This is protected by strand.
    uint64_t sent_fee_{ max_uint64 };
    bool requested_{};
    bool lowered_{};
};

} // namespace node
} // namespace libbitcoin

#endif
