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
#include <bitcoin/node/protocols/protocol.hpp>

#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

// Properties.
// ----------------------------------------------------------------------------

query& protocol::archive() const NOEXCEPT
{
    return session_->archive();
}

const node::configuration& protocol::node_config() const NOEXCEPT
{
    return session_->node_config();
}

const system::settings& protocol::system_settings() const NOEXCEPT
{
    return session_->system_settings();
}

const database::settings& protocol::database_settings() const NOEXCEPT
{
    return session_->database_settings();
}

////const network::settings& protocol::network_settings() const NOEXCEPT
////{
////    return session_->network_settings();
////}

const node::settings& protocol::node_settings() const NOEXCEPT
{
    return session_->node_settings();
}

bool protocol::is_current(bool confirmed) const NOEXCEPT
{
    return session_->is_current(confirmed);
}

} // namespace node
} // namespace libbitcoin
