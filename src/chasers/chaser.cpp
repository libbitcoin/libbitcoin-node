/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser.hpp>

#include <mutex>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

using namespace network;
using namespace system::chain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser::chaser(full_node& node) NOEXCEPT
  : node_(node),
    strand_(node.service().get_executor()),
    top_checkpoint_height_(node.config().bitcoin.top_checkpoint().height()),
    reporter(node.log)
{
}

// Methods.
// ----------------------------------------------------------------------------

void chaser::stopping(const code&) NOEXCEPT
{
}

void chaser::stop() NOEXCEPT
{
}

bool chaser::closed() const NOEXCEPT
{
    return node_.closed();
}

bool chaser::suspended() const NOEXCEPT
{
    return node_.suspended();
}

code chaser::fault(const code& ec) NOEXCEPT
{
    node_.fault(ec);
    return ec;
}

void chaser::resume() NOEXCEPT
{
    node_.resume();
}

code chaser::snapshot(const store::event_handler& handler) NOEXCEPT
{
    return node_.snapshot(handler);
}

code chaser::reload(const store::event_handler& handler) NOEXCEPT
{
    return node_.reload(handler);
}

lock chaser::get_reorganization_lock() NOEXCEPT
{
    return node_.get_reorganization_lock();
}

// Events.
// ----------------------------------------------------------------------------

object_key chaser::subscribe_events(event_notifier&& handler) NOEXCEPT
{
    return node_.subscribe_events(std::move(handler));
}

void chaser::notify(const code& ec, chase event_,
    event_value value) const NOEXCEPT
{
    node_.notify(ec, event_, value);
}

void chaser::notify_one(object_key key, const code& ec, chase event_,
    event_value value) const NOEXCEPT
{
    node_.notify_one(key, ec, event_, value);
}

// Strand.
// ----------------------------------------------------------------------------

asio::strand& chaser::strand() NOEXCEPT
{
    return strand_;
}

bool chaser::stranded() const NOEXCEPT
{
    return strand_.running_in_this_thread();
}

// Properties.
// ----------------------------------------------------------------------------

const node::configuration& chaser::config() const NOEXCEPT
{
    return node_.config();
}

query& chaser::archive() const NOEXCEPT
{
    return node_.archive();
}

bool chaser::is_current() const NOEXCEPT
{
    return node_.is_current();
}

bool chaser::is_current(uint32_t timestamp) const NOEXCEPT
{
    return node_.is_current(timestamp);
}

// get_timestamp error results in false (ok).
bool chaser::is_current(const database::header_link& link) const NOEXCEPT
{
    uint32_t timestamp{};
    return archive().get_timestamp(timestamp, link) &&
        node_.is_current(timestamp);
}

bool chaser::is_under_checkpoint(size_t height) const NOEXCEPT
{
    return height <= checkpoint();
}

size_t chaser::checkpoint() const NOEXCEPT
{
    return top_checkpoint_height_;
}

// Position.
// ----------------------------------------------------------------------------

size_t chaser::position() const NOEXCEPT
{
    // Called from start.
    ////BC_ASSERT(stranded());
    return position_;
}

void chaser::set_position(size_t height) NOEXCEPT
{
    // Called from start.
    ////BC_ASSERT(stranded());
    position_ = height;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
