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
#include <bitcoin/node/chasers/chaser_storage.hpp>

#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_storage

using namespace system;
using namespace network;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_storage::chaser_storage(full_node& node) NOEXCEPT
  : chaser(node)
{
}

code chaser_storage::start() NOEXCEPT
{
    disk_timer_ = std::make_shared<deadline>(log, strand(), seconds{1});

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_storage::handle_event(const code& ec, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::snapshot:
        {
            if (ec != database::error::disk_full)
                break;

            POST(do_full, height_t{});
            break;
        }
        case chase::stop:
        {
            POST(do_stop, height_t{});
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// monitor
// ----------------------------------------------------------------------------

void chaser_storage::do_full(size_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    handle_timer(error::success);
}

void chaser_storage::do_stop(size_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    disk_timer_->stop();
}

// utility
// ----------------------------------------------------------------------------

void chaser_storage::handle_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed() || ec == network::error::operation_canceled)
        return;

    if (ec != network::error::operation_timeout)
    {
        LOGN("Storage chaser timer, " << ec.message());
        return;
    }

    // Network is resumed or store is failed, cancel monitor.
    if (!suspended() || archive().is_fault())
        return;

    // Disk now has space, reset store condition and resume network.
    if (!is_full())
    {
        reset_full();
        resume_network();
        return;
    }

    // Otherwise restart the timer.
    disk_timer_->start(BIND(handle_timer, _1));
}

bool chaser_storage::is_full() const NOEXCEPT
{
    // TODO: difference required and available space.
    // TODO: after a remap (allocate) failure the map may be closed.
    // en.cppreference.com/w/cpp/filesystem/space_info
    return true;
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
