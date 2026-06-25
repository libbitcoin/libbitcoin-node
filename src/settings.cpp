/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/settings.hpp>

#include <algorithm>
#include <filesystem>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/estimator.hpp>

using namespace bc::system;
using namespace bc::network;

namespace libbitcoin {
namespace node {

settings::settings() NOEXCEPT
  : threads{ 1 },
    delay_inbound{ true },
    headers_first{ true },
    memory_priority{ true },
    thread_priority{ true },
    allow_overlapped{ true },
    allow_batch_race{ true },
    batch_signatures{ 100'000 },
    minimum_fee_rate{ 0.0 },
    minimum_bump_rate{ 0.0 },
    allowed_deviation{ 1.5 },
    announcement_cache{ 42 },
    fee_estimate_horizon{ 0 },
    ////snapshot_bytes{ 200'000'000'000 },
    ////snapshot_valid{ 250'000 },
    ////snapshot_confirm{ 500'000 },
    maximum_height{ 0 },
    silent_start_height{ 0xffffffff_u32 },
    maximum_concurrency{ 50'000 },
    sample_period_seconds{ 10 },
    currency_window_minutes{ 1440 },
    warn_dirty_background_ratio{ 90_u16 },
    warn_dirty_ratio{ 90_u16 }
{
}

settings::settings(chain::selection) NOEXCEPT
  : settings()
{
    // TODO: testnet, etc. maximum_concurrency.
}

size_t settings::threads_() const NOEXCEPT
{
    return std::max<size_t>(threads, one);
}

size_t settings::maximum_height_() const NOEXCEPT
{
    return to_bool(maximum_height) ? maximum_height : max_size_t;
}

size_t settings::maximum_concurrency_() const NOEXCEPT
{
    return to_bool(maximum_concurrency) ? maximum_concurrency : max_size_t;
}

size_t settings::fee_estimate_horizon_() const NOEXCEPT
{
    return std::min<size_t>(fee_estimate_horizon, estimator::maximum_horizon);
}

bool settings::fee_estimate_enabled() const NOEXCEPT
{
    return to_bool(fee_estimate_horizon_());
}

bool settings::batch_signatures_enabled() const NOEXCEPT
{
    return to_bool(batch_signatures);
}

network::steady_clock::duration settings::sample_period() const NOEXCEPT
{
    return network::seconds(sample_period_seconds);
}

network::wall_clock::duration settings::currency_window() const NOEXCEPT
{
    return network::minutes(currency_window_minutes);
}

network::processing_priority settings::thread_priority_() const NOEXCEPT
{
    // medium is "normal" (os default), so true is a behavior change.
    // highest is too much, as it makes the opreating system UI unresponsive.
    return thread_priority ? network::processing_priority::high :
        network::processing_priority::medium;
}

network::memory_priority settings::memory_priority_() const NOEXCEPT
{
    // highest is "normal" (os default), so false is a behavior change.
    // highest is the OS default, so far low does not have a noticeable effect.
    return memory_priority ? network::memory_priority::highest :
        network::memory_priority::low;
}

} // namespace node
} // namespace libbitcoin
