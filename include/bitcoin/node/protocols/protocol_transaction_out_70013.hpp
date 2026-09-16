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
        network::tracker<protocol_transaction_out_70013>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// Handle chaser events.
    bool handle_chase(const code& ec, chase event_,
        event_value value) NOEXCEPT override;

    /// Capture the peer's minimum fee rate for announcements.
    virtual bool handle_receive_fee_filter(const code& ec,
        const network::messages::peer::fee_filter::cptr& message) NOEXCEPT;

    /// Bypasses announcement of a tx below the peer's minimum fee rate.
    bool do_announce(transaction_t link) NOEXCEPT override;

private:
    bool insufficient(const database::fee_rate& rate) const NOEXCEPT;
    void do_send_fee_filter() NOEXCEPT;

    // These are protected by strand.
    uint64_t minimum_fee_{};
    uint64_t sent_fee_{};
};

} // namespace node
} // namespace libbitcoin

#endif
