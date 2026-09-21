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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_70013_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_IN_70013_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_in_70001.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_in_70013
  : public protocol_transaction_in_70001,
    protected network::tracker<protocol_transaction_in_70013>
{
public:
    typedef std::shared_ptr<protocol_transaction_in_70013> ptr;

    protocol_transaction_in_70013(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_transaction_in_70001(session, channel),
        network::tracker<protocol_transaction_in_70013>(session->log)
    {
    }

protected:
    /// The peer is advised of the fee rate and of suspension (see fee_filter).
    void do_handle_submit(const code& ec,
        const gate_t::ptr& gate) NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#endif
