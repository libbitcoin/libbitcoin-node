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

// TODO: fold into node::protocol now that this is trivial.

namespace libbitcoin {
namespace node {

#define CLASS protocol_observer

using namespace std::placeholders;

void protocol_observer::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3),
        BIND(complete_event, _1, _2));

    protocol::start();
}

// protected
void protocol_observer::complete_event(const code& ec, object_key) NOEXCEPT
{
    POST(do_complete_event, ec);
}

// private
void protocol_observer::do_complete_event(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // stopped() is true before stopping() is called (by base).
    if (stopped(ec))
    {
        unsubscribe_events();
        return;
    }
}

// If this is invoked before do_complete_event then it will unsubscribe.
void protocol_observer::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol::stopping(ec);
}

bool protocol_observer::handle_event(const code& ec, chase event_,
    event_value) NOEXCEPT
{
    if (stopped(ec))
        return false;

    switch (event_)
    {
        case chase::suspend:
        {
            stop(error::suspended_channel);
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

} // namespace node
} // namespace libbitcoin
