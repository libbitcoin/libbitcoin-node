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
#include <bitcoin/node/protocols/protocol_header_out_70014.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_out_70014

using namespace system;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start
// ----------------------------------------------------------------------------

void protocol_header_out_70014::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(send_compact, handle_receive_send_compact, _1, _2);
    protocol_header_out_70012::start();
}

// Outbound (cmpctblock).
// ----------------------------------------------------------------------------

bool protocol_header_out_70014::do_announce(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return false;

    if (wants_compact_blocks())
    {
        // Don't announce to peer that announced to us.
        const auto hash = archive().get_header_key(link);
        if (was_announced(hash))
            return true;

        if (const auto message = make_compact_block(link))
        {
            LOGN("Announce compact ..."
                << encode_hash(hash).substr(hash_size - 8, 8) << " to ["
                << opposite() << "].");
            NOTIFY(*message, handle_send, _1);
            return true;
        }
    }

    return !announce_headers() || protocol_header_out_70012::do_announce(link);
}

// Inbound (sendcmpct).
// ----------------------------------------------------------------------------

bool protocol_header_out_70014::handle_receive_send_compact(const code& ec,
    const send_compact::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // The signal is recorded on the channel by the version protocol.
    if ((message->compact_version == send_compact::compact_version_2) &&
        message->high_bandwidth)
        announce();

    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
