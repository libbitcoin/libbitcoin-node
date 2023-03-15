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
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in_31800::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in_31800");

    if (started())
        return;

    SUBSCRIBE_CHANNEL2(headers, handle_receive_headers, _1, _2);
    SEND1(create_get_headers(), handle_send, _1);
    protocol::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

bool protocol_header_in_31800::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_address_in_31402");

    if (stopped(ec))
        return false;

    LOGP("Received (" << message->header_ptrs.size() << ") headers from ["
        << authority() << "].");

    // TODO: optimize header hashing using read buffer in message deserialize.
    // Store each header, drop channel if fails (missing parent).
    for (const auto& header: message->header_ptrs)
    {
        // TODO: maintain context progression and store with header.
        if (!archive().set(*header, database::context{}))
        {
            stop(network::error::protocol_violation);
            return false;
        }
    };

    // Protocol requires max_get_headers unless complete.
    if (message->header_ptrs.size() == max_get_headers)
    {
        SEND1(create_get_headers({ message->header_ptrs.back()->hash() }),
            handle_send, _1);
    }

    return true;
}

// private
get_headers protocol_header_in_31800::create_get_headers() NOEXCEPT
{
    return create_get_headers(archive().get_hashes(get_blocks::heights(
        archive().get_top_candidate())));
}

// private
get_headers protocol_header_in_31800::create_get_headers(
    hashes&& hashes) NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request headers after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

} // namespace node
} // namespace libbitcoin
