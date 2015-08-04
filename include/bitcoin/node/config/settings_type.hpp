/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_SETTINGS_TYPE_HPP
#define LIBBITCOIN_NODE_SETTINGS_TYPE_HPP

#include <cstdint>
#include <string>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/config/settings.hpp>

namespace libbitcoin {
namespace node {
    
// TODO: rename to configuration
class BCN_API settings_type
{
public:
    settings_type()
    {
    }

    settings_type(
        const bc::node::settings& node_settings,
        const bc::chain::settings& chain_settings,
        const bc::network::settings& network_settings)
      : node(node_settings),
        chain(chain_settings),
        network(network_settings)
    {
    }

    // HACK: generalize logging.
    virtual std::string log_to_skip() const
    {
        return "";
    }

    // Convenience.
    virtual size_t minimum_start_height() const
    {
        return chain.checkpoints.empty() ? 0 : 
            chain.checkpoints.back().height();
    }

    // This allows timeouts to be const.
    virtual void initialize_timeouts()
    {
        using boost::posix_time::minutes;
        using boost::posix_time::seconds;

        timeouts.expiration = minutes(network.channel_expiration_minutes);
        timeouts.inactivity = minutes(network.channel_timeout_minutes);
        timeouts.heartbeat = minutes(network.channel_heartbeat_minutes);
        timeouts.handshake = minutes(network.channel_handshake_minutes);
        timeouts.revival = minutes(network.channel_revival_minutes);
        timeouts.connection = seconds(network.connect_timeout_seconds);
    }

    // settings
    bc::node::settings node;
    bc::chain::settings chain;
    bc::network::settings network;

    // Convenience.
    bc::network::timeout timeouts;
};

} // namespace node
} // namespace libbitcoin

#endif
