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
#include <bitcoin/network.hpp>
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
    bool headers_first;
    bool priority_validation;
    bool concurrent_validation;
    bool concurrent_confirmation;
    float allowed_deviation;
    uint16_t allocation_multiple;
    uint64_t snapshot_bytes;
    uint32_t snapshot_valid;
    uint32_t snapshot_confirm;
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
} // namespace libbitcoin

#endif
