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
#include <bitcoin/node/sessions/session_outbound.hpp>

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/protocols/protocols.hpp>

namespace libbitcoin {
namespace node {

session_outbound::session_outbound(full_node& node,
    uint64_t identifier) NOEXCEPT
  : attach(node, identifier)
{
    // attach(node, identifier)...
    // Set the current top for version protocol, before handshake.
    // Attach and execute appropriate version protocol.
};

    // Overrides session::performance.
bool session_outbound::performance(size_t) const NOEXCEPT
{
    return true;
}

void session_outbound::attach_protocols(
    const network::channel::ptr& channel) NOEXCEPT
{
    // Attach appropriate alert, reject, ping, and/or address protocols.
    network::session_outbound::attach_protocols(channel);

    auto& self = *this;
    constexpr auto performance = true;
    channel->attach<protocol_block_in>(self, performance)->start();
    ////channel->attach<protocol_block_out>(self)->start();
    ////channel->attach<protocol_transaction_in>(self)->start();
    ////channel->attach<protocol_transaction_out>(self)->start();
}

} // namespace node
} // namespace libbitcoin
