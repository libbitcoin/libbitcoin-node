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
#include <bitcoin/node/protocols/protocol_electrum.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_electrum
#define SUBSCRIBE_RPC(...) SUBSCRIBE_CHANNEL(void, __VA_ARGS__)

using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_electrum::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_RPC(handle_blockchain_headers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_relayfee, _1, _2);
    node::protocol_rpc<rpc_interface>::start();
}

// Handlers.
// ----------------------------------------------------------------------------

bool protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe) NOEXCEPT
{
    // TODO:
    return !ec;
}

bool protocol_electrum::handle_blockchain_relayfee(const code& ec,
    rpc_interface::blockchain_relayfee) NOEXCEPT
{
    // TODO:
    return !ec;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
