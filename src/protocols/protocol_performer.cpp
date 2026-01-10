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
#include <bitcoin/node/protocols/protocol_performer.hpp>

#include <chrono>
#include <bitcoin/node/protocols/protocol.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
    
#define CLASS protocol_performer

using namespace system;
using namespace network;
using namespace std::chrono;
using namespace std::placeholders;

void protocol_performer::start_performance() NOEXCEPT
{
    if (stopped())
        return;

    if (enabled_)
    {
        bytes_ = zero;
        start_ = steady_clock::now();
        performance_timer_->start(BIND(handle_performance_timer, _1));
    }
}

void protocol_performer::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec == network::error::operation_canceled ||
        ec == network::error::service_stopped)
        return;

    if (stopped())
        return;

    if (ec)
    {
        LOGF("Performance timer failure, " << ec.message());
        stop(ec);
        return;
    }

    if (is_idle())
    {
        // Channel is exhausted, performance no longer relevant.
        pause_performance();
        return;
    }

    // Submit performance to (outbound session) aggregate monitor in bytes/sec.
    send_performance(floored_divide(bytes_, greater(sign_cast<uint64_t>(
        duration_cast<seconds>(steady_clock::now() - start_).count()), one)));
}

void protocol_performer::pause_performance() NOEXCEPT
{
    BC_ASSERT(stranded());
    send_performance(max_uint64);
}

void protocol_performer::stop_performance() NOEXCEPT
{
    BC_ASSERT(stranded());
    send_performance(zero);
}

void protocol_performer::send_performance(uint64_t rate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (enabled_)
    {
        // Must come first as this takes priority as per configuration.
        // Shared performance manager detects slow and stalled channels.
        if (deviation_)
        {
            performance_timer_->stop();
            performance(rate, BIND(handle_send_performance, _1));
            return;
        }

        // Protocol performance manager detects only stalled channels.
        const auto ec = is_zero(rate) ? error::stalled_channel :
            (rate == max_uint64 ? error::exhausted_channel :
                error::success);

        performance_timer_->stop();
        do_handle_performance(ec);
    }
}

void protocol_performer::handle_send_performance(const code& ec) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_handle_performance, ec);
}

void protocol_performer::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // Caused only by performance(max) - had no outstanding work.
    // Timer stopped until chase::download event restarts it.
    if (ec == error::exhausted_channel)
        return;

    // Caused only by performance(zero|xxx) - had outstanding work.
    if (ec == error::stalled_channel || ec == error::slow_channel)
    {
        LOGP("Channel dropped [" << opposite() << "] " << ec.message());
        stop(ec);
        return;
    }

    if (ec)
    {
        LOGF("Performance failure [" << opposite() << "] " << ec.message());
        stop(ec);
        return;
    }

    // Restart performance timing cycle.
    start_performance();
}

void protocol_performer::count(size_t bytes) NOEXCEPT
{
    BC_ASSERT(stranded());
    bytes_ = ceilinged_add(bytes_, possible_wide_cast<uint64_t>(bytes));
}

} // namespace node
} // namespace libbitcoin
