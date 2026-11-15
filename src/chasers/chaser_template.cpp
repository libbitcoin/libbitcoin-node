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
#include <bitcoin/node/chasers/chaser_template.hpp>

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_template

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_template::chaser_template(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

code chaser_template::start() NOEXCEPT
{
    SUBSCRIBE_CHASE(handle_chase, _1, _2);
    return error::success;
}

// methods
// ----------------------------------------------------------------------------

void chaser_template::get_template(template_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_get_template, std::move(handler));
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_template::handle_chase(const code&, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    // Incoming events may already be flushed to the strand at this point.
    if (suspended())
        return true;

    switch (to_chase(value))
    {
        case chase::transaction:
        case chase::organized:
        case chase::reorganized:
        {
            POST(do_update);
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

// Pool changes during a build coalesce into one following build.
void chaser_template::do_update() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (building_)
    {
        stale_ = true;
        return;
    }

    build();
}

void chaser_template::do_get_template(const template_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());
    handler(error::success, height_, links_);
}

// build
// ----------------------------------------------------------------------------

// private
void chaser_template::build() NOEXCEPT
{
    BC_ASSERT(stranded());

    building_ = true;
    stale_ = false;
    request_template(BIND(handle_build, _1, _2, _3));
}

// private
void chaser_template::handle_build(const code& ec, size_t height,
    const database::tx_links& links) NOEXCEPT
{
    POST(do_build, ec, height, links);
}

// private
void chaser_template::do_build(const code& ec, size_t height,
    const database::tx_links& links) NOEXCEPT
{
    BC_ASSERT(stranded());

    building_ = false;
    if (closed() ||
        ec == network::error::service_stopped ||
        ec == error::pooling_disabled)
        return;

    if (ec)
    {
        fault(ec);
        return;
    }

    links_ = links;
    height_ = height;
    notify(error::success, chases::template_{ height });

    if (stale_)
        build();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
