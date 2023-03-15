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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_header_in
  : public node::protocol, network::tracker<protocol_header_in>
{
public:
    typedef std::shared_ptr<protocol_header_in> ptr;

    template <typename Session>
    protocol_header_in(Session& session, const channel_ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_header_in>(session.log())
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    ////virtual void send_send_headers() NOEXCEPT;
    virtual bool handle_receive_headers(const code& ec,
        const network::messages::headers::cptr& message) NOEXCEPT;

private:
    network::messages::get_headers create_get_headers() NOEXCEPT;
    network::messages::get_headers create_get_headers(
        system::hashes&& start_hashes) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
