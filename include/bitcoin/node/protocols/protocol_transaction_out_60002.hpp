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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_60002_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_60002_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out_106.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_out_60002
  : public protocol_transaction_out_106,
    protected network::tracker<protocol_transaction_out_60002>
{
public:
    typedef std::shared_ptr<protocol_transaction_out_60002> ptr;

    protocol_transaction_out_60002(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_transaction_out_106(session, channel),
        enable_memory_pool_(session->network_settings().enable_memory_pool),
        network::tracker<protocol_transaction_out_60002>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    /// True if the tx is not announced to the peer.
    virtual bool is_filtered(transaction_t link) NOEXCEPT;

    /// Announce the unconfirmed pool txs (bip35).
    virtual bool handle_receive_memory_pool(const code& ec,
        const network::messages::peer::memory_pool::cptr& message) NOEXCEPT;
    virtual void send_memory_pool(const code& ec,
        const database::pool_link& cursor, const database::pool_link& end,
        const gate_t::ptr& gate) NOEXCEPT;

private:
    // This is thread safe.
    const bool enable_memory_pool_;
};

} // namespace node
} // namespace libbitcoin

#endif
