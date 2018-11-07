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
#ifndef LIBBITCOIN_NODE_SESSION_HPP
#define LIBBITCOIN_NODE_SESSION_HPP

#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Intermediate session base class template.
/// This avoids having to make network::session into a template.
template <class Session>
class BCN_API session
  : public Session
{
protected:
    /// Construct an instance.
    session(full_node& node, bool notify_on_connect)
      : Session(node, notify_on_connect), node_(node)
    {
    }

    /// Attach a protocol to a channel, caller must start the channel.
    template <class Protocol, typename... Args>
    typename Protocol::ptr attach(network::channel::ptr channel,
        Args&&... args)
    {
        return std::make_shared<Protocol>(node_, channel,
            std::forward<Args>(args)...);
    }

private:

    // This is thread safe.
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
