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
#include <bitcoin/node/protocols/protocol_block_out_70014.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_out_70014

using namespace system;
using namespace network;
using namespace network::messages::peer;
using namespace std::placeholders;

BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_block_out_70014::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(get_compact_transactions,
        handle_receive_get_compact_transactions, _1, _2);

    protocol_block_out_70012::start();
}

// Inbound (get_data).
// ----------------------------------------------------------------------------

// Compact items are answered with compact blocks, others as blocks.
bool protocol_block_out_70014::handle_receive_get_data(const code& ec,
    const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!protocol_block_out_70012::handle_receive_get_data(ec, message))
        return false;

    const auto& query = archive();
    for (const auto& item: message->items)
    {
        if (!item.is_type(inventory_item::type_id::compact) &&
            !item.is_type(inventory_item::type_id::witness_compact))
            continue;

        const auto link = query.to_header(item.hash);
        if (!is_servable(item.hash, link))
            continue;

        const auto block = make_compact_block(link);
        if (!block)
        {
            LOGF("Compact block " << encode_hash(item.hash) << " not made.");
            continue;
        }

        SEND(*block, handle_send, _1);
    }

    return true;
}

// Inbound (getblocktxn).
// ----------------------------------------------------------------------------

bool protocol_block_out_70014::handle_receive_get_compact_transactions(
    const code& ec, const get_compact_transactions::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = query.to_header(message->block_hash);
    if (!is_servable(message->block_hash, link))
        return true;

    const auto txs = query.to_transactions(link);
    compact_transactions out{ message->block_hash, {} };
    out.transaction_ptrs.reserve(message->indexes.size());

    // Requested indexes are differentially encoded (bip152).
    size_t position{};
    for (size_t index{}; index < message->indexes.size(); ++index)
    {
        const auto offset = limit<size_t>(message->indexes.at(index));
        position = is_zero(index) ? offset :
            ceilinged_add(add1(position), offset);

        if (position >= txs.size())
        {
            stop(network::error::protocol_violation);
            return false;
        }

        const auto tx = query.get_transaction(txs.at(position), true);
        if (!tx)
        {
            stop(fault(error::protocol2));
            return false;
        }

        out.transaction_ptrs.push_back(tx);
    }

    SEND(std::move(out), handle_send, _1);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
