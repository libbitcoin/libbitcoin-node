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
#include <bitcoin/node/sessions/session_outbound.hpp>

#include <bitcoin/node/chase.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_outbound

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start/stop sequence.
// ----------------------------------------------------------------------------

void session_outbound::start(network::result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    update_connections();
    base::start(BIND(handle_started, _1, std::move(handler)));
}

void session_outbound::handle_started(const code& ec,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec || stopped() || is_zero(node_settings().current_connections))
    {
        handler(ec);
        return;
    }

    subscribe_chase(BIND(handle_chase, _1, _2));
    update_connections();
    handler(ec);
}

// Currency.
// ----------------------------------------------------------------------------

bool session_outbound::handle_chase(const code&, event_value value) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return false;

    switch (to_chase(value))
    {
        case chase::block:
        case chase::stale:
        {
            update_connections();
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Outbound is reduced when current, when inbound is enabled (delay_inbound).
void session_outbound::update_connections() NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto reduced = node_settings().current_connections;
    const auto configured = network_settings().outbound.connections;
    set_connections((is_recent() && to_bool(reduced)) ? reduced : configured);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
