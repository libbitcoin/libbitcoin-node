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
#include <bitcoin/node/protocols/protocol_header_in_70012.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_70012

using namespace network::messages;
using namespace std::placeholders;

void protocol_header_in_70012::complete() NOEXCEPT
{
    BC_ASSERT(stranded());
    ////protocol_header_in_31800::complete();

    if (!subscribed_)
    {
        SEND(send_headers{}, handle_send, _1);
        LOGP("Requested header announcements from [" << authority() << "].");
        subscribed_ = true;
    }
}

} // namespace node
} // namespace libbitcoin
