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
#include <bitcoin/node/full_node.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/sessions/sessions.hpp>

namespace libbitcoin {
namespace node {

full_node::full_node(const node::configuration& configuration) NOEXCEPT
  : p2p(configuration.network),
    configuration_(configuration)
{
}

void full_node::start(result_handler&& handler) NOEXCEPT
{
    ////// TODO: handle this in database start.
    ////if (!database::file::is_directory(configuration_.database.dir))
    ////{
    ////    handler(system::error::not_found);
    ////    return;
    ////}

    p2p::start(std::move(handler));
}

void full_node::run(result_handler&& handler) NOEXCEPT
{
    p2p::run(std::move(handler));
}

const node::configuration& full_node::configuration() const NOEXCEPT
{
    return configuration_;
}

network::session_manual::ptr full_node::attach_manual_session() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "attach_manual_session");
    return attach<node::session_manual>(*this);
}

network::session_inbound::ptr full_node::attach_inbound_session() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "attach_inbound_session");
    return attach<node::session_inbound>(*this);
}

network::session_outbound::ptr full_node::attach_outbound_session() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "attach_outbound_session");
    return attach<node::session_outbound>(*this);
}

} // namespace node
} // namespace libbitcoin
