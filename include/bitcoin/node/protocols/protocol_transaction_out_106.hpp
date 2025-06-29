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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_106_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_106_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_transaction_out_106
  : public node::protocol,
    protected network::tracker<protocol_transaction_out_106>
{
public:
    typedef std::shared_ptr<protocol_transaction_out_106> ptr;

    template <typename SessionPtr>
    protocol_transaction_out_106(const SessionPtr& session,
        const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        node_witness_(session->config().network.witness_node()),
        network::tracker<protocol_transaction_out_106>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

    /// The channel is stopping (called on strand by stop subscription).
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handle chaser events.
    virtual bool handle_event(const code& ec, chase event_, 
        event_value value) NOEXCEPT;

    /// Process tx announcement.
    virtual bool do_organized(transaction_t link) NOEXCEPT;

    virtual bool handle_receive_get_data(const code& ec,
        const network::messages::get_data::cptr& message) NOEXCEPT;
    virtual void send_transaction(const code& ec, size_t index,
        const network::messages::get_data::cptr& message) NOEXCEPT;

private:
    // These are thread safe.
    const bool node_witness_;
};

} // namespace node
} // namespace libbitcoin

#endif
