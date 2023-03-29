/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>

#include <chrono>
#include <utility>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;
using namespace std::chrono;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in_31800::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in_31800");

    if (started())
        return;

    // get_chain_state is always CANDIDATE.
    state_ = archive().get_chain_state(config().bitcoin,
        archive().get_top_candidate());

    if (!state_)
    {
        LOGF("Unexpected error, state not initialized.");
        return;
    }

    // There is one persistent common headers subscription.
    SUBSCRIBE_CHANNEL2(headers, handle_receive_headers, _1, _2);
    SEND1(create_get_headers(), handle_send, _1);
    protocol::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

bool protocol_header_in_31800::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_header_in_31800");

    if (stopped(ec))
        return false;

    const auto& coin = config().bitcoin;
    const auto& checks = coin.checkpoints;

    LOGP("Headers (" << message->header_ptrs.size() << ") from ["
        << authority() << "].");

    // Store each header, drop channel if invalid.
    for (const auto& header_ptr: message->header_ptrs)
    {
        if (stopped())
            return false;

        const auto& header = *header_ptr;
        code error = header.check(coin.timestamp_limit_seconds,
            coin.proof_of_work_limit, coin.scrypt_proof_of_work);

        if (error)
        {
            LOGR("Invalid header [" << encode_hash(header.hash()) << "] from ["
                << authority() << "] " << error.message());
            stop(network::error::protocol_violation);
            return false;
        }

        const auto height = add1(state_->height());
        if (chain::checkpoint::is_conflict(checks, header.hash(), height))
        {
            LOGR("Checkpoint conflict [" << encode_hash(header.hash())
                << "] from [" << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }

        // Rolling forward chain_state eliminates database cost.
        // get_chain_state doubles time cost, from ~15 to ~30 secs.
        state_.reset(new chain::chain_state(*state_, header, coin));
        error = header.accept(state_->context());

        if (!state_)
        {
            LOGR("Orphan header [" << encode_hash(header.hash())
                << "] from [" << authority() << "] " << error.message());
            stop(network::error::protocol_violation);
            return false;
        }

        const auto link = archive().set_link(header,
        {
            state_->forks(),
            possible_narrow_cast<uint32_t>(height),
            state_->median_time_past()
        });

        // TODO: Early parent check can be implemented by chain_state.
        // This allows it to be pre-verified without hitting the store.
        if (link.is_terminal())
        {
            LOGF("Orphan header [" << encode_hash(header.hash())
                << "] from [" << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }

        if (!archive().push_candidate(link))
        {
            LOGF("Push candidate error [" << encode_hash(header.hash())
                << "] from [" << authority() << "].");
            stop(network::error::protocol_violation);
            return false;
        }

        // This will be incorrect with multiple peers or block protocol.
        // archive().header_records() is a weak proxy for current height (top).
        reporter::fire(event_header, archive().header_records());
    }

    // Protocol presumes max_get_headers unless complete.
    if (message->header_ptrs.size() == max_get_headers)
    {
        SEND1(create_get_headers({ message->header_ptrs.back()->hash() }),
            handle_send, _1);
    }
    else
    {
        // Currency assumes empty response from peer if caught up at 2000.
        current();
    }

    return true;
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but thissignals initial currency.
void protocol_header_in_31800::current() NOEXCEPT
{
    // This will be incorrect with multiple peers or block protocol.
    // archive().header_records() is a weak proxy for current height (top).
    const auto top = archive().header_records();
    reporter::fire(event_current_headers, top);
    LOGN("Headers from [" << authority() << "] complete at (" << top << ").");
}

// private
get_headers protocol_header_in_31800::create_get_headers() NOEXCEPT
{
    // header sync is always CANDIDATEs.
    return create_get_headers(archive().get_candidate_hashes(
        get_headers::heights(archive().get_top_candidate())));
}

// private
get_headers protocol_header_in_31800::create_get_headers(
    hashes&& hashes) NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request headers after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
