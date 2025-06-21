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
#ifndef LIBBITCOIN_NODE_SESSIONS_SESSION_INBOUND_HPP
#define LIBBITCOIN_NODE_SESSIONS_SESSION_INBOUND_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/attach.hpp>

namespace libbitcoin {
namespace node {

class session_inbound
  : public attach<network::session_inbound>
{
public:
    typedef std::shared_ptr<session_inbound> ptr;
    using base = attach<network::session_inbound>;
    using base::base;

    bool disabled() const NOEXCEPT override;
};

} // namespace node
} // namespace libbitcoin

#endif
