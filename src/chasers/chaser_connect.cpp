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
#include <bitcoin/node/chasers/chaser_connect.hpp>

#include <functional>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_connect
    
using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_connect::chaser_connect(full_node& node) NOEXCEPT
  : chaser(node)
{
}

code chaser_connect::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());

    last_ = archive().get_top_confirmable();
    LOGN("Connected through block [" << last_ << "].");
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_connect::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::checked)
    {
        BC_ASSERT(std::holds_alternative<height_t>(value));
        POST(handle_checked, std::get<height_t>(value));
    }
}

void chaser_connect::handle_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // TODO: It may not be possible to do this on height alone, since an
    // asynchronous reorganization may render the height ambiguous.
    // TODO: height <= last_ implies reorganization.

    if (add1(last_) == height)
    {
        // TODO: the notification tells us the first height is associated.
        // TODO: so there is no need to check is_associated here on first.
        for (auto link = query.to_candidate(height);
            query.is_associated(link);
            link = query.to_candidate(++height))
        {
            // TODO: manage preconfirmable blocks.
            // TODO: may want to avoid reading this here (perf).
            ////const auto ec = query.get_block_state(link);
            ////if (ec == database::error::block_unconfirmable)
            ////{
            ////    notify(ec, chase::unconnected, height);
            ////    return;
            ////}

            // TODO: validate all block txs and get context.
            // Mark transactions as valid.
            ////if (!query.set_tx_disconnected({}, {}, {}, {}))
            ////if (!query.set_tx_preconnected({}, {}, {}, {}))
            ////if (!query.set_tx_connected({}, {}, {}, {}))
            ////{
            ////    // TODO: error::store_integrity.
            ////    return;
            ////}

            // TODO: validate block and get fees.
            // Mark block as valid and potentially confirmable.
            ////if (!query.set_block_unconfirmable(link))
            ////if (!query.set_block_preconfirmable(link))
            if (!query.set_block_confirmable(link, {}))
            {
                // TODO: error::store_integrity.
                return;
            }

            ////const auto last = possible_narrow_cast<height_t>(++last_);
            ////notify(error::success, chase::connected, last);
            ////LOGN("Connected block [" << last_ << "].");
            fire(events::event_archive, last_);
        }
    }
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
