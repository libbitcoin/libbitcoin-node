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
#include <bitcoin/node/protocols/protocol_observer.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_observer

using namespace std::placeholders;

void protocol_observer::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous.
    async_subscribe_events(BIND(handle_event, _1, _2, _3));

    protocol::start();
}

void protocol_observer::handle_event(const code&, chase event_,
    event_link) NOEXCEPT
{
    if (stopped())
        return;

    switch (event_)
    {
        case chase::pause:
        {
            POST(do_pause, channel_t{});
            break;
        }
        case chase::resume:
        {
            POST(do_resume, channel_t{});
            break;
        }
        case chase::start:
        ////case chase::pause:
        ////case chase::resume:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::block:
        case chase::header:
        case chase::download:
        case chase::checked:
        case chase::unchecked:
        case chase::preconfirmable:
        case chase::unpreconfirmable:
        case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        case chase::stop:
        {
            break;
        }
    }
}

void protocol_observer::do_pause(channel_t) NOEXCEPT
{
    pause();
}

void protocol_observer::do_resume(channel_t) NOEXCEPT
{
    resume();
}

} // namespace node
} // namespace libbitcoin
