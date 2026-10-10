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
#include <bitcoin/node/protocols/protocol_transaction_in_70016.hpp>

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_in_70016

using namespace system;

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Identity (bip339).
// ----------------------------------------------------------------------------

type_id protocol_transaction_in_70016::inventory_type() const NOEXCEPT
{
    return type_id::wtxid;
}

type_id protocol_transaction_in_70016::get_data_type() const NOEXCEPT
{
    return type_id::wtxid;
}

hash_digest protocol_transaction_in_70016::tx_identifier(
    const chain::transaction& tx) const NOEXCEPT
{
    return tx.get_hash(true);
}

bool protocol_transaction_in_70016::is_archived(
    const hash_digest& hash) const NOEXCEPT
{
    return !archive().to_witness_tx(hash).is_terminal();
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
