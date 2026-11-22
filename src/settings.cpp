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
#include <bitcoin/node/settings.hpp>

#include <filesystem>
#include <bitcoin/node/define.hpp>

using namespace bc::system;
using namespace bc::network;

namespace libbitcoin {
namespace node {

settings::transaction_pool::transaction_pool() NOEXCEPT
  : minimum_fee_rate{ 0.000001 },
    cutoff_rate{ 0 },
    maximum_outcomes{ 1'024 },
    bytes_per_sigop{ 20 },
    coinbase_weight{ 2'000 },
    coinbase_sigops{ 400 },
    maximum_chunk_depth{ 25 },
    estimate_cache_seconds{ 7 },
    estimate_room{ 1'000 },
    thread_priority{ true }
{
}

// The configured rate is btc/vkb, as reported to clients (see relayfee).
uint64_t settings::transaction_pool::minimum_fee_rate_() const NOEXCEPT
{
    // Ceilinged, as a configured minimum is not rounded down.
    return to_ceilinged_integer<uint64_t>(minimum_fee_rate *
        chain::satoshi_per_bitcoin);
}

// The configured rate is btc/vkb, converted as the minimum is.
uint64_t settings::transaction_pool::cutoff_rate_() const NOEXCEPT
{
    return to_ceilinged_integer<uint64_t>(cutoff_rate *
        chain::satoshi_per_bitcoin);
}

network::steady_clock::duration
settings::transaction_pool::estimate_cache() const NOEXCEPT
{
    return network::seconds(estimate_cache_seconds);
}

network::processing_priority
settings::transaction_pool::thread_priority_() const NOEXCEPT
{
    return thread_priority ? network::processing_priority::high :
        network::processing_priority::medium;
}

settings::settings() NOEXCEPT
  : network{ chain::selection::none },
    pool{},
    threads{ 0 },
    delay_inbound{ true },
    thread_priority{ true },
    provide_blocks{ true },
    require_blocks{ true },
    provide_witness{ true },
    require_witness{ true },
    provide_filters{ false },
    provide_privacy{ true },
    limited_blocks{ false },
    use_gpu{ true },
    cpu_batch_verify{ false },
    cpu_batch_silent{ false },
    allowed_deviation{ 1.5 },
    batch_verify{ 1'000'000 },
    batch_silent{ 1'000'000 },
    current_connections{ 10 },
    manual_relay_slots{ max_uint16 },
    outbound_relay_slots{ max_uint16 },
    ////snapshot_bytes{ 200'000'000'000 },
    ////snapshot_valid{ 250'000 },
    ////snapshot_confirm{ 500'000 },
    maximum_height{ 0 },
    maximum_concurrency{ 50'000 },
    silent_start_height{ 0xffffffff_u32 },
    sample_period_seconds{ 10 },
    compact_timeout_seconds{ 10 },
    compact_missing_percent{ 50 },
    currency_window_minutes{ 1440 }
{
}

settings::settings(chain::selection context) NOEXCEPT
  : settings()
{
    network = context;

    // Test network blocks are small, so the download window is raised.
    switch (context)
    {
        case chain::selection::testnet3:
        {
            maximum_concurrency = 1'000'000;
            break;
        }

        case chain::selection::testnet4:
        case chain::selection::testnet5:
        {
            maximum_concurrency = 100'000;
            break;
        }

        default:
        {
        }
    }
}

size_t settings::threads_() const NOEXCEPT
{
    // Zero implies hardware threads (including hyperthreads).
    return to_bool(threads) ? threads : network::cores();
}

size_t settings::maximum_height_() const NOEXCEPT
{
    return to_bool(maximum_height) ? maximum_height : max_size_t;
}

size_t settings::maximum_concurrency_() const NOEXCEPT
{
    return to_bool(maximum_concurrency) ? maximum_concurrency : max_size_t;
}

uint64_t settings::services_provided() const NOEXCEPT
{
    using namespace network::messages::peer;
    uint64_t services{ service::node_none };

    if (provide_blocks)
        services = bit_or<uint64_t>(services, limited_blocks ?
            service::node_network_limited : service::node_network);

    if (provide_witness)
        services = bit_or<uint64_t>(services, service::node_witness);

    if (provide_filters)
        services = bit_or<uint64_t>(services, service::node_client_filters);

    if (provide_privacy)
        services = bit_or<uint64_t>(services, service::node_encrypted_transport);

    return services;
}

uint64_t settings::services_required() const NOEXCEPT
{
    using namespace network::messages::peer;
    uint64_t services{ service::node_none };

    if (require_blocks)
        services = bit_or<uint64_t>(services, service::node_network);

    if (require_witness)
        services = bit_or<uint64_t>(services, service::node_witness);

    return services;
}

bool settings::use_gpu_() const NOEXCEPT
{
    return use_gpu && system::batched::accelerated();
}

bool settings::batch_verify_enabled() const NOEXCEPT
{
    return to_bool(batch_verify);
}

bool settings::batch_silent_enabled() const NOEXCEPT
{
    return to_bool(batch_silent);
}

bool settings::batch_verify_() const NOEXCEPT
{
    return batch_verify_enabled() && (cpu_batch_verify || use_gpu_());
}

bool settings::batch_silent_() const NOEXCEPT
{
    return batch_silent_enabled() && (cpu_batch_silent || use_gpu_());
}

network::steady_clock::duration settings::sample_period() const NOEXCEPT
{
    return network::seconds(sample_period_seconds);
}

network::steady_clock::duration settings::compact_timeout() const NOEXCEPT
{
    return network::seconds(compact_timeout_seconds);
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

} // namespace node
} // namespace libbitcoin
