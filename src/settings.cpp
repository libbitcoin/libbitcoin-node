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
#include <bitcoin/node/settings.hpp>

#include <algorithm>
#include <filesystem>

using namespace bc::system;
using namespace bc::network;

namespace libbitcoin {

// log
// ----------------------------------------------------------------------------

namespace log {

// Log states default to network compiled states or explicit false.
settings::settings() NOEXCEPT
  : application{ levels::application_defined },
    news{ levels::news_defined },
    session{ levels::session_defined },
    protocol{ false /*levels::protocol_defined*/ },
    proxy{ false /*levels::proxy_defined*/ },
    remote{ levels::remote_defined },
    fault{ levels::fault_defined },
    quitting{ false /*levels::quitting_defined*/ },
    objects{ false /*levels::objects_defined*/ },
    verbose{ false /*levels::verbose_defined*/ },
    maximum_size{ 1'000'000_u32 }
{
}

settings::settings(chain::selection) NOEXCEPT
  : settings()
{
}

std::filesystem::path settings::log_file1() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return path / "bn_end.log";
    BC_POP_WARNING()
}

std::filesystem::path settings::log_file2() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return path / "bn_begin.log";
    BC_POP_WARNING()
}

std::filesystem::path settings::events_file() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return path / "events.log";
    BC_POP_WARNING()
}

} // namespace log

// node
// ----------------------------------------------------------------------------

namespace node {

settings::settings() NOEXCEPT
  : priority{ true },
    delay_inbound{ true },
    headers_first{ true },
    allowed_deviation{ 1.5 },
    announcement_cache{ 42 },
    allocation_multiple{ 20 },
    ////snapshot_bytes{ 200'000'000'000 },
    ////snapshot_valid{ 250'000 },
    ////snapshot_confirm{ 500'000 },
    maximum_height{ 0 },
    maximum_concurrency{ 50'000 },
    sample_period_seconds{ 10 },
    currency_window_minutes{ 60 },
    threads{ 1 }
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

network::steady_clock::duration settings::sample_period() const NOEXCEPT
{
    return network::seconds(sample_period_seconds);
}

network::wall_clock::duration settings::currency_window() const NOEXCEPT
{
    return network::minutes(currency_window_minutes);
}

network::thread_priority settings::priority_() const NOEXCEPT
{
    return priority ? network::thread_priority::high :
        network::thread_priority::normal;
}

} // namespace node

// server
// ----------------------------------------------------------------------------

namespace server {

// settings::settings
settings::settings(system::chain::selection, const embedded_pages& explore,
    const embedded_pages& web) NOEXCEPT
  : explore("explore", explore),
    web("web", web)    
{
}

// settings::embedded_pages
span_value settings::embedded_pages::css()  const NOEXCEPT { return {}; }
span_value settings::embedded_pages::html() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::ecma() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::font() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::icon() const NOEXCEPT { return {}; }
bool settings::embedded_pages::enabled() const NOEXCEPT
{
    return !html().empty();
}

// settings::html_server
settings::html_server::html_server(const std::string_view& logging_name,
    const embedded_pages& embedded) NOEXCEPT
  : network::settings::http_server(logging_name),
    pages(embedded)
{
}

system::string_list settings::html_server::origin_names() const NOEXCEPT
{
    // secure changes default port from 80 to 443.
    const auto port = secure ? http::default_tls : http::default_http;
    return network::config::to_host_names(hosts, port);
}

bool settings::html_server::enabled() const NOEXCEPT
{
    return (!path.empty() || pages.enabled()) && http_server::enabled();
}

} // namespace server

// ----------------------------------------------------------------------------

} // namespace libbitcoin
