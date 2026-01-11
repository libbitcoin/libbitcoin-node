/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_31800

using namespace system;
using namespace network;
using namespace network::messages::peer;
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
    protocol_peer::start();
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

    LOGP("Headers (" << message->header_ptrs.size() << ") from ["
        << opposite() << "].");

    // Store each header, drop channel if invalid.
    for (const auto& ptr: message->header_ptrs)
    {
        if (stopped())
            return false;

        if (subscribed)
            set_announced(ptr->get_hash());

        // A job backlog will occur when organize is slower than download.
        // This is not likely with headers-first even for high channel count.
        organize(ptr, BIND(handle_organize, _1, _2, ptr));
    }

    // The headers response to get_headers is limited to max_get_headers.
    if (message->header_ptrs.size() == max_get_headers)
    {
        const auto& last = message->header_ptrs.back()->get_hash();
        SEND(create_get_headers(last), handle_send, _1);
    }
    else
    {
        // Protocol presumes max_get_headers unless complete.
        // Completeness assumes empty response from peer if caught up at 2000.
        LOGP("Completed headers from [" << opposite() << "].");
        complete();
    }

    return true;
}

// not stranded
void protocol_header_in_31800::handle_organize(const code& ec,
    size_t height, const chain::header::cptr& LOG_ONLY(header_ptr)) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped ||
        ec == error::duplicate_header)
        return;

    // Assuming no store failure this is an orphan or consensus failure.
    if (ec)
    {
        if (is_zero(height))
        {
            LOGP("Header [" << encode_hash(header_ptr->get_hash()) << "] from ["
                << opposite() << "] " << ec.message());
        }
        else
        {
            LOGR("Header [" << encode_hash(header_ptr->get_hash()) << ":"
                << height << "] from [" << opposite() << "] " << ec.message());
        }

        stop(ec);
        return;
    }

    LOGP("Header [" << encode_hash(header_ptr->get_hash()) << ":" << height
        << "] from [" << opposite() << "] " << ec.message());
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals peer completeness.
void protocol_header_in_31800::complete() NOEXCEPT
{
    BC_ASSERT(stranded());

    // There are no header announcements at 31800, so translate from inv.
    if (!subscribed && is_current(true))
    {
        subscribed = true;
        SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
        LOGP("Subscribed to block announcements at [" << opposite() << "].");
    }
}

// Inbound (inv).
// ----------------------------------------------------------------------------
// Handle announcement by sending get_headers() if missing any announced.

bool protocol_header_in_31800::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // bip144: get_data uses witness type_id but inv does not.

    const auto& query = archive();
    for (const auto& item: message->view(type_id::block))
    {
        if (!query.is_block(item.hash))
        {
            // This is inefficient but simple and limited to protocol 31800.
            // Since subscribed is set, the response headers will be cached.
            SEND(create_get_headers(), handle_send, _1);
            return true;
        }
    }

    return true;
}

// utilities
// ----------------------------------------------------------------------------

get_headers protocol_header_in_31800::create_get_headers() const NOEXCEPT
{
    // Header sync is from the archived (strong) candidate chain.
    // Until the header tree is current the candidate chain remains empty.
    // So all channels will fully sync from the top candidate at their startup.
    const auto& query = archive();
    const auto index = get_headers::heights(query.get_top_candidate());
    return create_get_headers(query.get_candidate_hashes(index));
}

get_headers protocol_header_in_31800::create_get_headers(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_headers(hashes{ last });
}

get_headers protocol_header_in_31800::create_get_headers(
    hashes&& hashes) const NOEXCEPT
{
    if (hashes.empty())
        return {};

    if (is_one(hashes.size()))
    {
        LOGP("Request headers after [" << encode_hash(hashes.front())
            << "] from [" << opposite() << "].");
    }
    else
    {
        LOGP("Request headers (" << hashes.size()
            << ") after [" << encode_hash(hashes.front())
            << "] from [" << opposite() << "].");
    }

    return { std::move(hashes) };
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
