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

#include <functional>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

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
    BC_ASSERT(node_stranded());

    ////top_ = archive().get_top_confirmable();
    ////LOGN("Confirmed through block [" << top_ << "].");

    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_confirm::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::connected)
    {
        BC_ASSERT(std::holds_alternative<height_t>(value));
        POST(handle_connected, std::get<height_t>(value));
    }
}

// TODO: handle new strong connected branch (may issue 'confirmed'/'unconfirmed').
void chaser_confirm::handle_connected(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // TODO: It may not be possible to do this on height alone, since an
    // asynchronous reorganization may render the height ambiguous.
    // TODO: height <= last_ implies reorganization.

    if (add1(top_) == height)
    {
        // TODO: determine if stronger (reorg).
        // TODO: pop/push new strong block and confirm.
        // TODO: if any fails restore to previous state.

        // Associate valid txs with confirmed block.
        ////if (!query.set_unstrong(link))
        ////if (!query.set_strong(query.to_candidate(height)))
        ////{
        ////    // TODO: error::store_integrity.
        ////    return;
        ////}

        // Push block to confirmed index.
        if (!query.push_confirmed(query.to_candidate(top_)))
        {
            // TODO: error::store_integrity.
            return;
        }

        const auto top = possible_narrow_cast<height_t>(++top_);
        notify(error::success, chase::confirmed, top);

        ////LOGN("Confirmed block [" << top_ << "].");
        fire(events::event_block, top_);
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
