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
    configuration(system::chain::selection context) NOEXCEPT;

    /// Read files and pack derived settings, must follow configuration parse.
    virtual code initialize() NOEXCEPT;

    /// Relay slots reserved for manual peers, then for outbound peers.
    virtual size_t manual_relay_slots() const NOEXCEPT;
    virtual size_t outbound_relay_slots() const NOEXCEPT;

    /// Settings.
    system::settings bitcoin;
    database::settings database;
    network::settings network;
    node::settings node;
};

} // namespace node
} // namespace libbitcoin

#endif
