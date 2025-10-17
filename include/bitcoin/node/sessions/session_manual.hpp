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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_MANUAL_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_MANUAL_HPP

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/session_peer.hpp>

namespace libbitcoin {
namespace node {

class BCN_API session_manual
  : public session_peer<network::session_manual>
{
public:
    typedef std::shared_ptr<session_manual> ptr;
    using base = session_peer<network::session_manual>;
    using base::base;
};

} // namespace node
} // namespace libbitcoin

#endif
