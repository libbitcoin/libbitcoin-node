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

#include <algorithm>
#include <cmath>
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
};

// Overrides session::performance.
bool session_outbound::performance(size_t bytes) NOEXCEPT
{
    // TODO: pass completion handler and bounce to do_performance().
    ////BC_ASSERT_MSG(stranded(), "session_outbound");

    const auto count = config().network.outbound_connections;
    speeds_.push_front(static_cast<double>(bytes));
    speeds_.resize(count);
    const auto mean = std::accumulate(speeds_.begin(), speeds_.end(), 0.0) / count;
    const auto vary = std::accumulate(speeds_.begin(), speeds_.end(), 0.0,
        [mean](auto sum, auto speed) NOEXCEPT
        {
            const auto difference = speed - mean;
            return sum + (difference * difference);
        }) / count;
    const auto standard_deviation = std::sqrt(vary);

    // TODO: return false via completion handler if bytes less than N SD.
    return standard_deviation >= 0.0;
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
