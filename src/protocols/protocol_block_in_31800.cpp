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
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>

#include <chrono>
#include <functional>
#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Performance polling.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected channel strand");

    if (stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        LOGF("Performance timer error, " << ec.message());
        stop(ec);
        return;
    }

    // Compute rate in bytes per second.
    const auto now = steady_clock::now();
    const auto gap = std::chrono::duration_cast<seconds>(now - start_).count();
    const auto rate = floored_divide(bytes_, gap);
    LOGN("Rate ["
        << identifier() << "] ("
        << bytes_ << "/"
        << gap << " = "
        << rate << ").");

    // Reset counters and log rate.
    bytes_ = zero;
    start_ = now;

    // Bounces to network strand, performs work, then calls handler.
    // Channel will continue to process blocks while this call excecutes on the
    // network strand. Timer will not be restarted until this call completes.
    performance(identifier(), rate, BIND1(handle_performance, ec));
}

void protocol_block_in_31800::handle_performance(const code& ec) NOEXCEPT
{
    POST1(do_handle_performance, ec);
}

void protocol_block_in_31800::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected network strand");

    if (stopped())
        return;

    // stalled_channel or slow_channel
    if (ec)
    {
        LOGF("Performance action, " << ec.message());
        stop(ec);
        return;
    };

    performance_timer_->start(BIND1(handle_performance_timer, _1));
}

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");

    if (started())
        return;

    const auto& query = archive();
    const auto top = query.get_top_candidate();
    top_ = { query.get_header_key(query.to_candidate(top)), top };

    if (report_performance_)
    {
        start_ = steady_clock::now();
        performance_timer_->start(BIND1(handle_performance_timer, _1));
    }

    SUBSCRIBE_CHANNEL2(block, handle_receive_block, _1, _2);
    protocol::start();
}

void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");
    performance_timer_->stop();
    protocol::stopping(ec);
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");

    if (stopped(ec))
        return false;

    // Asynchronous organization serves all channels.
    // A job backlog will occur when organize is slower than download.
    // This should not be a material issue given lack of validation here.
    check(message->block_ptr, BIND1(handle_check, _1));

    bytes_ += message->cached_size;

    // TODO: return true only if there are more blocks outstanding.
    return true;
}

void protocol_block_in_31800::handle_check(const code& ec) NOEXCEPT
{
    if (ec == network::error::service_stopped)
        return;

    // TODO: log result (LOGR/LOGP).

    stop(ec);
}

// private
// ----------------------------------------------------------------------------

get_data protocol_block_in_31800::create_get_data(
    const hashes&) const NOEXCEPT
{
    get_data getter{};

    // TODO: generate get_data request.

    return getter;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
