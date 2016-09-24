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
#ifndef LIBBITCOIN_NODE_SESSION_OUTBOUND_HPP
#define LIBBITCOIN_NODE_SESSION_OUTBOUND_HPP

#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Outbound connections session, thread safe.
class BCN_API session_outbound
  : public session<network::session_outbound>, track<session_outbound>
{
public:
    typedef std::shared_ptr<session_outbound> ptr;

    /// Construct an instance.
    session_outbound(full_node& network, blockchain::full_chain& blockchain);

protected:
    /// Overridden to attach blockchain protocols.
    void attach_protocols(network::channel::ptr channel) override;

    blockchain::full_chain& blockchain_;
};

} // namespace node
} // namespace libbitcoin

#endif
