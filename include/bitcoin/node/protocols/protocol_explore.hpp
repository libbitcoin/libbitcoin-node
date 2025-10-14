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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_EXPLORE_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_EXPLORE_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_explore
  : public network::protocol_html,
    protected network::tracker<protocol_explore>
{
public:
    typedef std::shared_ptr<protocol_explore> ptr;
    using options_t = network::settings::html_server;
    using channel_t = network::channel_http;

    protocol_explore(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : network::protocol_html(session, channel, options),
        ////options_(options),
        network::tracker<protocol_explore>(session->log)
    {
    }

protected:
    void handle_receive_get(const code& ec,
        const network::http::method::get& request) NOEXCEPT override;

private:
    // This is thread safe.
    ////const options_t& options_;
};

} // namespace node
} // namespace libbitcoin

#endif
