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
    // When the top preconfirmable block is stronger than the top confirmed
    // a reorganzation attempt occurs in which confirmability is established.
    /////top_ = archive().get_top_preconfirmable();

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_confirm::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    // These can come out of order, advance in order synchronously.
    switch (event_)
    {
        case chase::block:
        {
            BC_ASSERT(std::holds_alternative<size_t>(value));
            POST(do_preconfirmed, std::get<size_t>(value));
            break;
        }
        case chase::preconfirmed:
        {
            BC_ASSERT(std::holds_alternative<size_t>(value));
            POST(do_preconfirmed, std::get<size_t>(value));
            break;
        }
        case chase::disorganized:
        {
            BC_ASSERT(std::holds_alternative<size_t>(value));
            POST(do_disorganized, std::get<size_t>(value));
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
        ////case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        ////case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

void chaser_confirm::do_disorganized(size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    top_ = top;

    // Assure consistency given chance of intervening candidate organization.
    do_preconfirmed(top);

    ////// TODO: need to deal with this at startup as well.
    ////// There may be associated above top_preconfirmable resulting in a stall.
    ////// Process above top_preconfirmable until unassociated.
    ////handle_preconfirmed(add1(top_));
}

void chaser_confirm::do_preconfirmed(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Does the height currently represent a strong branch?
    // In case of stronger branch reorganization the branch may become
    // invalidated during processing (popping). How is safety assured?
    // TODO: obtain link from height, obtain a fork_point, walk candidates from
    // TODO: link to fork point via parents, to top confirmed, comparing work.
    ////if (!is_strong_branch(height))
    ////    return;

    if (add1(top_) == height)
    {
        // TODO: determine if stronger (reorg).
        // TODO: Associate txs with confirmed block.
        // TODO: pop/push new strong block and confirm.
        // TODO: if any fails restore to previous state.
        ///////////////////////////////////////////////////////////////////////

        // Push block to confirmed index.
        const auto link = query.to_candidate(height);
        if (!query.push_confirmed(link))
        {
            close(error::store_integrity);
            return;
        }

        // TODO:
        if (false)
        {
            notify(error::success, chase::unconfirmed, link);
            fire(events::block_disorganized, height);
            fire(events::block_reorganized, height);
            fire(events::block_organized, height);
        }

        ++top_;
        notify(error::success, chase::confirmed, link);
        fire(events::block_confirmed, top_);
        fire(events::block_organized, top_);
        ///////////////////////////////////////////////////////////////////////
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
