/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

    bool verbose;
    uint32_t maximum_size;
    std::filesystem::path path;
#if defined (HAVE_MSC)
    std::filesystem::path symbols;
#endif

    virtual std::filesystem::path log_file1() NOEXCEPT;
    virtual std::filesystem::path log_file2() NOEXCEPT;
    virtual std::filesystem::path events_file() NOEXCEPT;
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

    // TODO: these aren't actually node settings.
    uint16_t target;
    uint16_t interval;
};

} // namespace node
} // namespace libbitcoin

#endif
