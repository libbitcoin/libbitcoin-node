/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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

#include <boost/filesystem.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

// Not localizable.
#define BN_HELP_VARIABLE "help"
#define BN_SETTINGS_VARIABLE "settings"
#define BN_VERSION_VARIABLE "version"

// This must be lower case but the env var part can be any case.
#define BN_CONFIG_VARIABLE "config"

// This must match the case of the env var.
#define BN_ENVIRONMENT_VARIABLE_PREFIX "BN_"

/// Full node configuration, thread safe.
class BCN_API configuration
{
public:
    configuration(config::settings context);

    /// Options.
    bool help;
    bool initchain;
    bool settings;
    bool version;

    /// Options and environment vars.
    boost::filesystem::path file;

    /// Settings.
    node::settings node;
    blockchain::settings chain;
    database::settings database;
    network::settings network;
    libbitcoin::settings bitcoin;
};

} // namespace node
} // namespace libbitcoin

#endif
