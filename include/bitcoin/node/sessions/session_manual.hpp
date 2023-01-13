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
#ifndef LIBBITCOIN_NODE_SESSION_MANUAL_HPP
#define LIBBITCOIN_NODE_SESSION_MANUAL_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Manual connections session, thread safe.
class BCN_API session_manual
  : public session<network::session_manual>, network::track<session_manual>
{
public:
    typedef std::shared_ptr<session_manual> ptr;

    session_manual(full_node& network) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
