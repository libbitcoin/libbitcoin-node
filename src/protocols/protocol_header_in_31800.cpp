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
#include <bitcoin/node/error.hpp>

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
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(headers, handle_receive_headers, _1, _2);
    SEND(create_get_headers(), handle_send, _1);
    protocol::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

// Each channel synchronizes its own header branch from startup to complete.
// Send get_headers and process responses in order until peer is exhausted.
bool protocol_header_in_31800::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    LOGP("Headers (" << message->header_ptrs.size() << ") from ["
        << authority() << "].");

    // Store each header, drop channel if invalid.
    for (const auto& header_ptr: message->header_ptrs)
    {
        if (stopped())
            return false;

        // Redundant query to avoid queuing up excess jobs.
        if (!query.is_header(header_ptr->hash()))
        {
            // Asynchronous organization serves all channels.
            organize(header_ptr, BIND(handle_organize, _1, _2, header_ptr));
        }
    }

    // Protocol presumes max_get_headers unless complete.
    // The headers response to get_headers is limited to max_get_headers.
    if (message->header_ptrs.size() == max_get_headers)
    {
        SEND(create_get_headers(message->header_ptrs.back()->hash()),
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
    BC_ASSERT(stranded());
    LOGN("Headers from [" << authority() << "] exhausted.");
}

void protocol_header_in_31800::handle_organize(const code& ec, size_t height,
    const chain::header::cptr& header_ptr) NOEXCEPT
{
    if (stopped() || ec == error::duplicate_header)
        return;

    if (ec)
    {
        // Assuming no store failure this is an orphan or consensus failure.
        LOGR("Header [" << encode_hash(header_ptr->hash())
            << "] at (" << height << ") from [" << authority() << "] "
            << ec.message());
        stop(ec);
        return;
    }

    LOGP("Header [" << encode_hash(header_ptr->hash())
        << "] at (" << height << ") from [" << authority() << "] "
        << ec.message());
}

// private
// ----------------------------------------------------------------------------

get_headers protocol_header_in_31800::create_get_headers() const NOEXCEPT
{
    // Header sync is from the archived (strong) candidate chain.
    // Until the header tree is current the candidate chain remains empty.
    // So all channels will fully sync from the top candidate at their startup.
    const auto& query = archive();
    return create_get_headers(query.get_candidate_hashes(get_headers::heights(
        query.get_top_candidate())));
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
