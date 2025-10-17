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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WEB_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_WEB_HPP

#include <memory>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_html.hpp>

namespace libbitcoin {
namespace node {

/// Administrative web site for the node (currently just page server).
class BCN_API protocol_web
  : public node::protocol_html,
    protected network::tracker<protocol_web>
{
public:
    typedef std::shared_ptr<protocol_web> ptr;

    protocol_web(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : node::protocol_html(session, channel, options),
        network::tracker<protocol_web>(session->log)
    {
    }

    /// Public start is required.
    void start() NOEXCEPT override
    {
        node::protocol_html::start();
    }
};

} // namespace node
} // namespace libbitcoin

#endif
