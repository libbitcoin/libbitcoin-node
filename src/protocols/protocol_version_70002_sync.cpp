/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_version_70002_sync.hpp>

#include <bitcoin/network.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::network;

protocol_version_70002_sync::protocol_version_70002_sync(p2p& network,
    channel::ptr channel)
  : protocol_version_70002(network, channel)
{
}

void protocol_version_70002_sync::send_version(const message::version& self)
{
    auto version = self;
    version.relay = false;
    version.services = message::version::service::none;
    version.address_sender.services = message::version::service::none;
    protocol_version_70002::send_version(version);
}

} // namespace node
} // namespace libbitcoin
