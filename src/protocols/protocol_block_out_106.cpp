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
#include <bitcoin/node/protocols/protocol_block_out_106.hpp>

#include <chrono>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_out_106

using namespace system;
using namespace network;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// This protocol provides witness support despite 106 being much older than
// bip144. This is because protocols are splot on the version negotiated in the
// p2p handshake. Witness is enabled via a service bit. Protocols should also
// be factored based on relevant service bits but that is not yet implemented.

// start/stop
// ----------------------------------------------------------------------------

void protocol_block_out_106::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_chase(BIND(handle_chase, _1, _2, _3));
    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    SUBSCRIBE_CHANNEL(get_blocks, handle_receive_get_blocks, _1, _2);
    protocol_peer::start();
}

void protocol_block_out_106::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_chase();
    protocol_peer::stopping(ec);
}

// handle events (block)
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped() || superseded())
        return false;

    switch (event_)
    {
        case chase::block:
        {
            // value is organized block pk.
            BC_ASSERT(std::holds_alternative<header_t>(value));
            POST(do_announce, std::get<header_t>(value));
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Outbound (block).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::superseded() const NOEXCEPT
{
    return false;
}

bool protocol_block_out_106::do_announce(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return false;

    // Don't announce to peer that announced to us.
    const auto hash = archive().get_header_key(link);
    if (was_announced(hash))
        return true;

    if (hash == null_hash)
    {
        ////stop(fault(system::error::not_found));
        LOGF("Organized block not found.");
        return true;
    }

    // bip144: get_data uses witness type_id but inv does not.
    const inventory inv{ { { type_id::block, hash } } };
    NOTIFY(inv, handle_send, _1);
    return true;
}

// Inbound (get_blocks).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_receive_get_blocks(const code& ec,
    const get_blocks::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    LOGP("Get blocks above " << encode_hash(message->start_hash())
        << " from [" << opposite() << "].");

    SEND(create_inventory(*message), handle_send, _1);
    return true;
}

// Inbound (get_data).
// ----------------------------------------------------------------------------

bool protocol_block_out_106::handle_receive_get_data(const code& ec,
    const get_data::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    if (!node_witness_ && message->any_witness())
    {
        LOGR("Unsupported witness get_data from [" << opposite() << "].");
        stop(network::error::protocol_violation);
        return false;
    }

    const auto size = message->count(get_data::selector::blocks);
    if (is_zero(size))
        return true;

    send_block(error::success, zero, message, gate());
    return true;
}

// Outbound (block).
// ----------------------------------------------------------------------------

void protocol_block_out_106::send_block(const code& ec, size_t index,
    const get_data::cptr& message, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    const auto& query = archive();

    // Drain unservable items, skipping non-block inventory. The derived
    // protocol accumulates them if it reports them, and otherwise stops the
    // channel on the first.
    database::header_link link{};
    for (; index < message->items.size(); ++index)
    {
        const auto& item = message->items.at(index);
        if (!item.is_block())
            continue;

        if (item.is_witness_type() && !node_witness_)
        {
            LOGR("Unsupported witness get_data from [" << opposite() << "].");
            stop(network::error::protocol_violation);
            return;
        }

        link = query.to_header(item.hash);
        if (is_servable(item, link))
            break;

        if (!handle_unservable(item))
            return;
    }

    // The report resumes this loop on completion, so it precedes the block.
    if (report_unservable(index, message, gate))
        return;

    if (index >= message->items.size())
        return;

    const auto& item = message->items.at(index);
    const auto witness = item.is_witness_type();
    const auto start = logger::now();
    messages::peer::block out
    {
        { query.get_wire_block(link, witness), witness }
    };

    // Association is verified above, so this is not ordinary peer input.
    if (!out.block.is_valid())
    {
        LOGV("Requested block " << encode_hash(item.hash) << " from ["
            << opposite() << "] not obtained.");

        if (handle_unservable(item))
            report_unservable(add1(index), message, gate);

        return;
    }

    span<microseconds>(events::block_usecs, start);
    SEND(std::move(out), send_block, _1, add1(index), message, gate);
}

// The checkpoint, milestone and association queries assume an archived header.
bool protocol_block_out_106::is_servable(const inventory_item& LOG_ONLY(item),
    const database::header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());

    // A hash that resolves to no header is ordinary peer input.
    if (link.is_terminal())
    {
        LOGV("Requested block " << encode_hash(item.hash) << " from ["
            << opposite() << "] not stored.");
        return false;
    }

    const auto& query = archive();
    if (node_pruned_ && (is_under_checkpoint(link) || query.is_milestone(link)))
    {
        LOGV("Requested pruned block " << encode_hash(item.hash)
            << " from [" << opposite() << "].");
        return false;
    }

    // This block could not have been advertised to the peer.
    if (!query.is_associated(link))
    {
        LOGV("Requested block " << encode_hash(item.hash) << " from ["
            << opposite() << "] not found.");
        return false;
    }

    return true;
}

// not_found is undefined below bip37, so the channel is stopped instead.
bool protocol_block_out_106::handle_unservable(
    const inventory_item& LOG_ONLY(item)) NOEXCEPT
{
    BC_ASSERT(stranded());

    LOGR("Unservable block " << encode_hash(item.hash) << " from ["
        << opposite() << "], stopping.");

    stop(system::error::not_found);
    return false;
}

// There is nothing to report below bip37, the channel is stopped above.
bool protocol_block_out_106::report_unservable(size_t,
    const get_data::cptr&, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    return false;
}

// utilities
// ----------------------------------------------------------------------------


protocol_block_out_106::inventory protocol_block_out_106::create_inventory(
    const get_blocks& locator) const NOEXCEPT
{
    // Empty response implies complete (success).
    if (!is_current_chain(true))
        return {};

    return inventory::factory
    (
        archive().get_blocks(locator.start_hashes, locator.stop_hash,
            network::messages::peer::max_get_blocks), type_id::block
    );
}

bool protocol_block_out_106::is_under_checkpoint(
    const database::header_link& link) NOEXCEPT
{
    const auto height = archive().get_height(link);
    if (height.is_terminal())
    {
        fault(database::error::integrity);
        return false;
    }

    return height <= top_checkpoint_height_;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
