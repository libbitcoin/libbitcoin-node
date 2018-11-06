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
#ifndef LIBBITCOIN_NODE_SETTINGS_HPP
#define LIBBITCOIN_NODE_SETTINGS_HPP

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// Common database configuration settings, properties not thread safe.
class BCN_API settings
{
public:
    settings();
    settings(config::settings context);

    /// Properties.
    float maximum_deviation;
    uint32_t block_latency_seconds;
    bool refresh_transactions;

    /// Helpers.
    asio::duration block_latency() const;
};

} // namespace node
} // namespace libbitcoin

#endif
