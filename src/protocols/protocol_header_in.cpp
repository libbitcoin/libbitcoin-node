/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_header_in.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in

using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in");

    if (started())
        return;

    SUBSCRIBE_CHANNEL2(headers, handle_receive_headers, _1, _2);

    // TODO: initial block locator.
    SEND1(get_headers{}, handle_send, _1);

    protocol::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

bool protocol_header_in::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_address_in_31402");

    if (stopped(ec))
        return false;

    LOGP("Received (" << message->header_ptrs.size() << ") headers from ["
        << authority() << "].");

    // TODO: next block locator.
    return true;
}

} // namespace node
} // namespace libbitcoin
