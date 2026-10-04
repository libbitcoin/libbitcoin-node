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
#include <bitcoin/node/protocols/protocol_peer.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace network;

// Organizers.
// ----------------------------------------------------------------------------

void protocol_peer::get_hashes(map_handler&& handler) NOEXCEPT
{
    session_->get_hashes(std::move(handler));
}

void protocol_peer::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    session_->put_hashes(map, std::move(handler));
}

// Methods.
// ----------------------------------------------------------------------------

void protocol_peer::performance(uint64_t speed,
    network::result_handler&& handler) const NOEXCEPT
{
    // Passed protocol->session->full_node->check_chaser.post->do_update.
    session_->performance(events_key(), speed, std::move(handler));
}

code protocol_peer::fault(const code& ec) NOEXCEPT
{
    // Short-circuit self stop.
    stop(ec);

    // Stop all other channels and suspend all connectors/acceptors.
    session_->fault(ec);
    return ec;
}

// Announcements.
// ----------------------------------------------------------------------------

void protocol_peer::set_announced(const system::hash_digest& hash) NOEXCEPT
{
    channel_->set_announced(hash);
}

void protocol_peer::set_current(bool value) NOEXCEPT
{
    channel_->set_current(value);
}

bool protocol_peer::was_announced(const system::hash_digest& hash) const NOEXCEPT
{
    return channel_->was_announced(hash);
}

// Events notification.
// ----------------------------------------------------------------------------

void protocol_peer::notify(const code& ec, event_value value) const NOEXCEPT
{
    session_->notify(ec, value);
}

void protocol_peer::notify_one(object_key key, const code& ec,
    event_value value) const NOEXCEPT
{
    session_->notify_one(key, ec, value);
}

// Compact blocks (bip152 version 2).
// ----------------------------------------------------------------------------

network::messages::peer::compact_block::cptr protocol_peer::make_compact_block(
    const database::header_link& link) const NOEXCEPT
{
    using namespace network::messages::peer;
    const auto& query = archive();
    const auto header = query.get_header(link);
    const auto txs = query.to_transactions(link);
    if (!header || txs.empty())
        return {};

    const auto coinbase = query.get_transaction(txs.front(), true);
    const auto wtxids = query.get_wtxids(link);
    if (!coinbase || wtxids.size() != txs.size())
        return {};

    const auto nonce = maybe_random::next<uint64_t>(0, max_uint64);
    const auto key = chain::short_id::to_key(*header, nonce);

    compact_block::short_id_list ids{};
    ids.reserve(sub1(wtxids.size()));
    for (auto it = std::next(wtxids.cbegin()); it != wtxids.cend(); ++it)
    {
        if (*it == null_hash)
            return {};

        const auto id = chain::short_id::to_id(key, *it);
        ids.push_back(chain::short_id::to_mini(id));
    }

    return to_shared(compact_block
    {
        header,
        nonce,
        std::move(ids),
        compact_block_items{ compact_block_item{ zero, coinbase } }
    });
}

} // namespace node
} // namespace libbitcoin
