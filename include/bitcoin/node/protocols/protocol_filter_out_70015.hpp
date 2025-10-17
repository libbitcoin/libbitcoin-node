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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_CLIENT_FILTER_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_CLIENT_FILTER_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_peer.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_filter_out_70015
  : public node::protocol_peer,
    protected network::tracker<protocol_filter_out_70015>
{
public:
    typedef std::shared_ptr<protocol_filter_out_70015> ptr;

    protocol_filter_out_70015(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol_peer(session, channel),
        network::tracker<protocol_filter_out_70015>(session->log)
    {
    }

    /// Start/stop protocol (strand required).
    void start() NOEXCEPT override;

protected:
    virtual bool handle_receive_get_filter_checkpoint(const code& ec,
        const network::messages::peer::get_client_filter_checkpoint::cptr& message) NOEXCEPT;
    virtual bool handle_receive_get_filter_headers(const code& ec,
        const network::messages::peer::get_client_filter_headers::cptr& message) NOEXCEPT;
    virtual bool handle_receive_get_filters(const code& ec,
        const network::messages::peer::get_client_filters::cptr& message) NOEXCEPT;

private:
    using ancestry_ptr = std::shared_ptr<database::header_links>;
    void send_filter(const code& ec, const ancestry_ptr& ancestry) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
