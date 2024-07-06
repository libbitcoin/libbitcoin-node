/**
 * Copyright (c) 2011-2024 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser_populate.hpp>

#include <functional>
#include <utility>
#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_populate

using namespace system;
using namespace system::chain;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_populate::chaser_populate(full_node& node) NOEXCEPT
  : chaser(node),
    threadpool_(std::max(node.config().node.threads, 1_u32)),
    independent_strand_(threadpool_.service().get_executor())
{
}

// start/stop
// ----------------------------------------------------------------------------

code chaser_populate::start() NOEXCEPT
{
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_populate::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
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

// populate
// ----------------------------------------------------------------------------

// Could also pass ctx.
void chaser_populate::populate(const block::cptr& block,
    const header_link& link, size_t height,
    network::result_handler&& complete) NOEXCEPT
{
    if (closed())
        return;

    // Unordered, but we may prefer to first populate from cache :|.
    /*bool*/ ////archive().populate(*block);

    boost::asio::post(independent_strand_,
        BIND(do_populate, block, link, height, std::move(complete)));
}

void chaser_populate::do_populate(const block::cptr& block,
    header_link::integer link, size_t height,
    const network::result_handler& complete) NOEXCEPT
{
    BC_ASSERT(independent_strand_.running_in_this_thread());

    // Previous blocks may not be archived.
    /*bool*/ ////archive().populate(*block);

    // Use all closure parameters to ensure they aren't optimized out.
    if (block->is_valid() && is_nonzero(height) &&
        link != header_link::terminal)
    {
        // Sends notification and deletes captured block in creating strand.
        // Notify coincident with delete ensures there is no cleanup backlog.
        complete(error::success);
    }
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
