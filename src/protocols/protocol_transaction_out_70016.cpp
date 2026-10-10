/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/node/protocols/protocol_transaction_out_70016.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_70016

using namespace system;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Identity (bip339).
// ----------------------------------------------------------------------------

type_id protocol_transaction_out_70016::inventory_type() const NOEXCEPT
{
    return type_id::wtxid;
}

hash_digest protocol_transaction_out_70016::tx_identifier(
    transaction_t link) const NOEXCEPT
{
    return archive().get_wtxid(link);
}

database::tx_link protocol_transaction_out_70016::to_transaction(
    const inventory_item& item) const NOEXCEPT
{
    if (item.is_type(type_id::wtxid))
        return archive().to_witness_tx(item.hash);

    return protocol_transaction_out_70013::to_transaction(item);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
