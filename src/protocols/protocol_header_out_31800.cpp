/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_header_out_31800.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_out_31800

using namespace system;
using namespace network;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_header_out_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(get_headers, handle_receive_get_headers, _1, _2);
    protocol_peer::start();
}

// Outbound (get_headers).
// ----------------------------------------------------------------------------

bool protocol_header_out_31800::handle_receive_get_headers(const code& ec,
    const get_headers::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    LOGP("Get headers above " << encode_hash(message->start_hash())
        << " from [" << opposite() << "].");

    SEND(create_headers(*message), handle_send, _1);
    return true;
}

// utilities
// ----------------------------------------------------------------------------

network::messages::peer::headers protocol_header_out_31800::create_headers(
    const get_headers& locator) const NOEXCEPT
{
    // Empty response implies complete (success).
    if (!is_current(true))
        return {};

    return
    {
        archive().get_headers(locator.start_hashes, locator.stop_hash,
            max_get_headers)
    };
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
