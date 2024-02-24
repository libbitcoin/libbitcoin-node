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
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>

#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in_31800::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in_31800");

    if (started())
        return;

    const auto& query = archive();
    const auto top = query.get_top_candidate();
    top_ = { query.get_header_key(query.to_candidate(top)), top };

    SUBSCRIBE_CHANNEL2(headers, handle_receive_headers, _1, _2);
    SEND1(create_get_headers(), handle_send, _1);
    protocol::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

// Send get_headers and process responses in order until peer is exhausted.
bool protocol_header_in_31800::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in_31800");

    if (stopped(ec))
        return false;

    LOGP("Headers (" << message->header_ptrs.size() << ") from ["
        << authority() << "].");

    // Store each header, drop channel if invalid.
    for (const auto& header_ptr: message->header_ptrs)
    {
        if (stopped())
            return false;

        if (header_ptr->previous_block_hash() != top_.hash())
        {
            // Out of order or invalid.
            LOGP("Orphan header [" << encode_hash(header_ptr->hash())
                << "] from [" << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }

        // Add header at next height.
        const auto height = add1(top_.height());

        // Asynchronous organization serves all channels.
        organize(header_ptr, BIND3(handle_organize, _1, height, header_ptr));

        // Set the new top and continue. Organize error will stop the channel.
        top_ = { header_ptr->hash(), height };
    }

    // Protocol presumes max_get_headers unless complete.
    if (message->header_ptrs.size() == max_get_headers)
    {
        SEND1(create_get_headers(message->header_ptrs.back()->hash()),
            handle_send, _1);
    }
    else
    {
        // Completeness assumes empty response from peer if caught up at 2000.
        complete();
    }

    return true;
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals peer completeness.
void protocol_header_in_31800::complete() NOEXCEPT
{
    LOGN("Headers from [" << authority() << "] complete at ("
        << top_.height() << ").");
}

void protocol_header_in_31800::handle_organize(const code& ec, size_t height,
    const chain::header::cptr& header_ptr) NOEXCEPT
{
    if (!ec)
    {
        LOGP("Header [" << encode_hash(header_ptr->hash())
            << "] at (" << height << ") from [" << authority() << "] "
            << ec.message());
        return;
    }

    // This may be either a remote error or store corruption.
    if (ec != network::error::service_stopped)
    {
        LOGR("Error organizing header [" << encode_hash(header_ptr->hash())
            << "] at (" << height << ") from [" << authority() << "] "
            << ec.message());
    }

    stop(ec);
}

// private
// ----------------------------------------------------------------------------

get_headers protocol_header_in_31800::create_get_headers() NOEXCEPT
{
    // Header sync is from the archived (strong) candidate chain.
    // Until the header tree is current the candidate chain remains empty.
    // So all channels will fully sync from the top candidate at their startup.
    return create_get_headers(archive().get_candidate_hashes(
        get_headers::heights(archive().get_top_candidate())));
}

get_headers protocol_header_in_31800::create_get_headers(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_headers(hashes{ last });
}

get_headers protocol_header_in_31800::create_get_headers(
    hashes&& hashes) const NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request headers after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
