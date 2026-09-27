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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_70014_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_BLOCK_OUT_70014_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_block_out_70012.hpp>

namespace libbitcoin {
namespace node {

/// Serve compact blocks and their txs (bip152 version 2).
class BCN_API protocol_block_out_70014
  : public protocol_block_out_70012,
    protected network::tracker<protocol_block_out_70014>
{
public:
    typedef std::shared_ptr<protocol_block_out_70014> ptr;

    protocol_block_out_70014(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_block_out_70012(session, channel),
        network::tracker<protocol_block_out_70014>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    using get_compact_transactions =
        network::messages::peer::get_compact_transactions;

    bool handle_receive_get_data(const code& ec,
        const get_data::cptr& message) NOEXCEPT override;
    virtual bool handle_receive_get_compact_transactions(const code& ec,
        const get_compact_transactions::cptr& message) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
