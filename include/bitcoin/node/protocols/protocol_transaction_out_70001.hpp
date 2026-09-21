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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70001_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_TRANSACTION_OUT_70001_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_transaction_out_106.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_transaction_out_70001
  : public protocol_transaction_out_106,
    protected network::tracker<protocol_transaction_out_70001>
{
public:
    typedef std::shared_ptr<protocol_transaction_out_70001> ptr;

    protocol_transaction_out_70001(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : protocol_transaction_out_106(session, channel),
        enable_not_found_(session->network_settings().enable_not_found),
        network::tracker<protocol_transaction_out_70001>(session->log)
    {
    }

protected:
    /// The item cannot be served, accumulates it for the not_found reply.
    bool handle_unservable(
        const network::messages::peer::inventory_item& item) NOEXCEPT override;

    /// Replies not_found with the accumulated items, false if none.
    bool report_unservable(size_t index,
        const network::messages::peer::get_data::cptr& message,
        const gate_t::ptr& gate) NOEXCEPT override;

private:
    // This is thread safe.
    const bool enable_not_found_;

    // This is protected by strand.
    inventory_items unservable_{};
};

} // namespace node
} // namespace libbitcoin

#endif
