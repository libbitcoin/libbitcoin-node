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
#ifndef LIBBITCOIN_NODE_PROTOCOL_HPP
#define LIBBITCOIN_NODE_PROTOCOL_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
    
class full_node;
class configuration;

class BCN_API protocol
  : public network::protocol
{
protected:
    typedef network::channel::ptr channel_ptr;

    protocol(const network::session& session, const channel_ptr& channel,
        full_node& node) NOEXCEPT;

    /// network::protocol also exposes network::settings.
    /// settings: bitcoin, database, blockchain, network, node.
    const configuration& config() const NOEXCEPT;

private:
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
