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

// Compact blocks (bip152 version 2).
// ----------------------------------------------------------------------------

// static
siphash_key protocol_peer::to_compact_key(const chain::header& header,
    uint64_t nonce) NOEXCEPT
{
    auto data = header.to_data();
    extend(data, to_little_endian(nonce));
    return to_siphash_key(split(sha256_hash(data)).first);
}

// static
uint64_t protocol_peer::to_short_id(const siphash_key& key,
    const hash_digest& wtxid) NOEXCEPT
{
    constexpr auto mask = unmask_right<uint64_t>(to_bits(mini_hash_size));
    return bit_and(siphash(key, wtxid), mask);
}

// static
uint64_t protocol_peer::from_mini(const mini_hash& id) NOEXCEPT
{
    data_array<sizeof(uint64_t)> bytes{};
    std::copy(id.begin(), id.end(), bytes.begin());
    return from_little_endian<uint64_t>(bytes);
}

// static
mini_hash protocol_peer::to_mini(uint64_t id) NOEXCEPT
{
    mini_hash out{};
    const auto bytes = to_little_endian(id);
    std::copy_n(bytes.begin(), out.size(), out.begin());
    return out;
}

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
    const auto key = to_compact_key(*header, nonce);

    compact_block::short_id_list ids{};
    ids.reserve(sub1(wtxids.size()));
    for (auto it = std::next(wtxids.begin()); it != wtxids.end(); ++it)
    {
        if (*it == null_hash)
            return {};

        ids.push_back(to_mini(to_short_id(key, *it)));
    }

    return to_shared(compact_block
    {
        header,
        nonce,
        std::move(ids),
        compact_block_items{ compact_block_item{ zero, coinbase } }
    });
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

} // namespace node
} // namespace libbitcoin
