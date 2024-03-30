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
#include <bitcoin/node/chasers/chaser_confirm.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_confirm

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_confirm::chaser_confirm(full_node& node) NOEXCEPT
  : chaser(node)
{
}

code chaser_confirm::start() NOEXCEPT
{
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_confirm::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    // These can come out of order, advance in order synchronously.
    using namespace system;
    switch (event_)
    {
        case chase::block:
        {
            POST(do_preconfirmed, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::preconfirmed:
        {
            POST(do_preconfirmed, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::header:
        case chase::download:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::pause:
        case chase::resume:
        case chase::bump:
        case chase::checked:
        case chase::unchecked:
        ////case chase::preconfirmed:
        case chase::unpreconfirmed:
        case chase::confirmed:
        case chase::unconfirmed:
        case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        ////case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

void chaser_confirm::do_preconfirmed(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // As each new validation arrives the fork point is identified.
    // Work is compared and if the greater then confirmeds are popped,
    // candidates are pushed, confirmed/unconfirmed, and so-on. If unconfirmed
    // revert to original confirmations, otherwise notify subscribers of reorg.


}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
