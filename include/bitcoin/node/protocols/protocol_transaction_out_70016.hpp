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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70016_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70016_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out_70013.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_out_70016
  : public protocol_transaction_out_70013,
    protected network::tracker<protocol_transaction_out_70016>
{
public:
    typedef std::shared_ptr<protocol_transaction_out_70016> ptr;

    protocol_transaction_out_70016(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_transaction_out_70013(session, channel),
        network::tracker<protocol_transaction_out_70016>(session->log)
    {
    }

protected:
    /// Transactions are announced, requested and served by witness hash.
    type_id inventory_type() const NOEXCEPT override;
    system::hash_digest identifier(transaction_t link) const NOEXCEPT override;
    database::tx_link to_transaction(
        const inventory_item& item) const NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#endif
