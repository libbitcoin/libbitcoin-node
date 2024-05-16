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
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_storage::chaser_storage(full_node& node) NOEXCEPT
  : chaser(node),
    store_(node.config().database.path)
{
}

// start
// ----------------------------------------------------------------------------

code chaser_storage::start() NOEXCEPT
{
    // Cosntruct is too early to create the unstarted timer.
    disk_timer_ = std::make_shared<deadline>(log, strand(), seconds{1});

    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

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

// monitoring events
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

    if (!have_capacity())
    {
        disk_timer_->start(BIND(handle_timer, _1));
        return;
    }

    do_reload();
}


void chaser_storage::do_reload() NOEXCEPT
{
    BC_ASSERT(stranded());

    // Disk now has space, reset store condition and resume network.
    const auto start = logger::now();
    if (const auto ec = reload([this](auto event_, auto table) NOEXCEPT
    {
        LOGN("reload::" << full_node::store::events.at(event_)
            << "(" << full_node::store::tables.at(table) << ")");
    }))
    {
        // No retry, otherwise it's just a tight loop.
        LOGF("Failed to reload from disk full condition, " << ec.message());
    }
    else
    {
        const auto span = duration_cast<seconds>(logger::now() - start);
        resume_network();

        LOGN("Reload from disk full complete in " << span.count() << " secs.");
    }
}

bool chaser_storage::have_capacity() const NOEXCEPT
{
    size_t have{};
    const auto required = archive().get_space();
    if (file::space(have, store_) && have >= required)
    {
        LOGN("Space required [" << required << "] and free [" << have << "].");
        return true;
    }

    LOGV("Space required [" << required << "] and free [" << have << "].");
    return false;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
