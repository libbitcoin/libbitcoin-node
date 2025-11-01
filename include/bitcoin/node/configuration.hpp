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
#ifndef LIBBITCOIN_NODE_CONFIGURATION_HPP
#define LIBBITCOIN_NODE_CONFIGURATION_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

/// Full node configuration, thread safe.
class BCN_API configuration
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(configuration);

    configuration(system::chain::selection context,
        const server::settings::embedded_pages& explore,
        const server::settings::embedded_pages& web) NOEXCEPT;

    /// Environment.
    std::filesystem::path file;

    /// Information.
    bool help{};
    bool hardware{};
    bool settings{};
    bool version{};

    /// Actions.
    bool newstore{};
    bool backup{};
    bool restore{};

    /// Chain scans.
    bool flags{};
    bool information{};
    bool slabs{};
    bool buckets{};
    bool collisions{};

    /// Ad-hoc Testing.
    bool test{};
    bool write{};

    /// Settings.
    log::settings log;
    server::settings server;
    node::settings node;
    network::settings network;
    database::settings database;
    system::settings bitcoin;
};

} // namespace node
} // namespace libbitcoin

#endif
