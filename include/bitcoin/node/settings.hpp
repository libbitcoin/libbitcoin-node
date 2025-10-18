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
#ifndef LIBBITCOIN_NODE_SETTINGS_HPP
#define LIBBITCOIN_NODE_SETTINGS_HPP

#include <filesystem>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace log {

/// [log] settings.
class BCN_API settings
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(settings);

    settings() NOEXCEPT;
    settings(system::chain::selection context) NOEXCEPT;

    bool application;
    bool news;
    bool session;
    bool protocol;
    bool proxy;
    bool remote;
    bool fault;
    bool quitting;
    bool objects;
    bool verbose;

    uint32_t maximum_size;
    std::filesystem::path path;

#if defined (HAVE_MSC)
    std::filesystem::path symbols;
#endif

    virtual std::filesystem::path log_file1() const NOEXCEPT;
    virtual std::filesystem::path log_file2() const NOEXCEPT;
    virtual std::filesystem::path events_file() const NOEXCEPT;
};

} // namespace log

namespace node {

/// [node] settings.
class BCN_API settings
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(settings);

    settings() NOEXCEPT;
    settings(system::chain::selection context) NOEXCEPT;

    /// Properties.
    bool priority;
    bool delay_inbound;
    bool headers_first;
    float allowed_deviation;
    uint16_t announcement_cache;
    uint16_t allocation_multiple;
    ////uint64_t snapshot_bytes;
    ////uint32_t snapshot_valid;
    ////uint32_t snapshot_confirm;
    uint32_t maximum_height;
    uint32_t maximum_concurrency;
    uint16_t sample_period_seconds;
    uint32_t currency_window_minutes;
    uint32_t threads;

    /// Helpers.
    virtual size_t threads_() const NOEXCEPT;
    virtual size_t maximum_height_() const NOEXCEPT;
    virtual size_t maximum_concurrency_() const NOEXCEPT;
    virtual network::steady_clock::duration sample_period() const NOEXCEPT;
    virtual network::wall_clock::duration currency_window() const NOEXCEPT;
    virtual network::thread_priority priority_() const NOEXCEPT;
};

} // namespace node

namespace server {

/// [server] settings.
class BCN_API settings
{
public:
    /// html (http/s) document server settings (has directory/default).
    /// This is for web servers that expose a local file system directory.
    struct html_server
      : public network::settings::http_server
    {
        /// Directory to serve.
        std::filesystem::path path{};

        /// Default page for default URL (recommended).
        std::string default_{ "index.html" };

        /// Validated against origins if configured (recommended).
        network::config::endpoints origins{};

        /// Normalized origins.
        system::string_list origin_names() const NOEXCEPT;

        /// !path.empty() && http_server::enabled() [hidden, not virtual]
        bool enabled() const NOEXCEPT;
    };

    DEFAULT_COPY_MOVE_DESTRUCT(settings);

    settings() NOEXCEPT {};
    settings(system::chain::selection) NOEXCEPT {};

    /// native admin web interface, isolated (http/s, stateless html)
    server::settings::html_server web{ "web" };

    /// native RESTful block explorer (http/s, stateless html/json)
    server::settings::html_server explore{ "explore" };

    /// native websocket query interface (http/s->tcp/s, json, handshake)
    network::settings::websocket_server socket{ "socket" };

    /// bitcoind compat interface (http/s, stateless json-rpc-v2)
    network::settings::http_server bitcoind{ "bitcoind" };

    /// electrum compat interface (tcp/s, json-rpc-v2)
    network::settings::tcp_server electrum{ "electrum" };

    /// stratum v1 compat interface (tcp/s, json-rpc-v1, auth handshake)
    network::settings::tcp_server stratum_v1{ "stratum_v1" };

    /// stratum v2 compat interface (tcp[/s], binary, auth/privacy handshake)
    network::settings::tcp_server stratum_v2{ "stratum_v2" };
};

} // namespace server
} // namespace libbitcoin

#endif
