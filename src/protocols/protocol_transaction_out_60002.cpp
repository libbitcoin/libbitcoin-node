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
#include <bitcoin/node/protocols/protocol_transaction_out_60002.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_transaction_out_60002

using namespace system;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// start
// ----------------------------------------------------------------------------

void protocol_transaction_out_60002::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    if (enable_memory_pool_)
    {
        SUBSCRIBE_CHANNEL(memory_pool, handle_receive_memory_pool, _1, _2);
    }

    protocol_transaction_out_106::start();
}

// Filter.
// ----------------------------------------------------------------------------

bool protocol_transaction_out_60002::is_filtered(transaction_t) NOEXCEPT
{
    return false;
}

// Inbound (mempool).
// ----------------------------------------------------------------------------

bool protocol_transaction_out_60002::handle_receive_memory_pool(
    const code& ec, const memory_pool::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    // Txs pooled after these are read are announced by the transaction event.
    get_confirmable_txs(BIND(handle_confirmable_txs, _1, _2, gate()));
    return true;
}

// not stranded
void protocol_transaction_out_60002::handle_confirmable_txs(const code& ec,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() || ec == network::error::service_stopped)
        return;

    POST(do_confirmable_txs, ec,
        std::make_shared<const database::tx_links>(links), gate);
}

// A closed pool has no txs to announce.
void protocol_transaction_out_60002::do_confirmable_txs(const code& ec,
    const links_ptr& links, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || ec == error::pooling_disabled)
        return;

    if (ec)
    {
        stop(ec);
        return;
    }

    send_memory_pool(error::success, links, zero, gate);
}

// Outbound (inv).
// ----------------------------------------------------------------------------

void protocol_transaction_out_60002::send_memory_pool(const code& ec,
    const links_ptr& links, size_t next, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    inventory_items items{};
    for (; items.size() < max_inventory && next < links->size(); ++next)
    {
        const auto link = links->at(next);
        if (is_filtered(link))
            continue;

        if (const auto hash = tx_identifier(link); hash != null_hash)
            items.emplace_back(inventory_type(), hash);
    }

    if (items.empty())
        return;

    SEND(inventory{ std::move(items) }, send_memory_pool, _1, links, next,
        gate);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
