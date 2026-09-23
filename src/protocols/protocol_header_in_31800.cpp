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
#include <bitcoin/node/protocols/protocol_header_in_31800.hpp>

#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_header_in_31800

using namespace system;
using namespace network;
using namespace network::messages::peer;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Sample budget when header rows are not configured (expected is zero).
constexpr size_t default_samples = 1024;

// Start.
// ----------------------------------------------------------------------------

void protocol_header_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_CHANNEL(headers, handle_receive_headers, _1, _2);
    SEND(create_get_headers(), handle_send, _1);
    protocol_peer::start();
}

// Inbound (headers).
// ----------------------------------------------------------------------------

// Each channel synchronizes its own header branch from startup to complete.
// Send get_headers and process responses in order until peer is exhausted.
bool protocol_header_in_31800::handle_receive_headers(const code& ec,
    const headers::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& ptrs = message->header_ptrs;
    LOGP("Headers (" << ptrs.size() << ") from [" << opposite() << "].");

    if (subscribed)
        for (const auto& ptr: ptrs)
            set_announced(ptr->get_hash());

    // Protocol presumes max_get_headers unless complete.
    // Completeness assumes empty response from peer if caught up at 2000.
    const auto full = (ptrs.size() == max_get_headers);
    if (archiving_)
        collect(*message, full);
    else
        synchronize(*message, full);

    return true;
}

// Validate and discard each header, sampling hashes, until proven.
void protocol_header_in_31800::synchronize(const headers& message,
    bool full) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& ptrs = message.header_ptrs;
    if (ptrs.empty())
    {
        finish();
        return;
    }

    // A message not extending the branch restarts from its stored parent.
    // An unstored parent (announcement) is requested from the candidate.
    const auto& first = ptrs.front()->previous_block_hash();
    if ((!state_ || first != state_->hash()) && !restart(first))
    {
        LOGP("Header [" << encode_hash(ptrs.front()->get_hash()) << "] from ["
            << opposite() << "] " << code{ error::orphan_header }.message());
        SEND(create_get_headers(), handle_send, _1);
        return;
    }

    const auto& settings = system_settings();
    for (const auto& ptr: ptrs)
    {
        const auto& header = *ptr;
        const auto& hash = header.get_hash();
        if (header.previous_block_hash() != state_->hash())
        {
            const code ec{ error::orphan_header };
            LOGR("Header [" << encode_hash(hash) << "] from [" << opposite()
                << "] " << ec.message());
            stop(ec);
            return;
        }

        const auto state = to_shared<chain_state>(*state_, header, settings);
        if (const auto ec = chaser_header::validate(header, *state, settings))
        {
            LOGR("Header [" << encode_hash(hash) << ":" << state->height()
                << "] from [" << opposite() << "] " << ec.message());
            stop(ec);
            return;
        }

        state_ = state;
        const auto height = state->height();
        if (is_zero(height % interval_))
            sample(hash);

        if (settings.milestone.equals(hash, height))
            milestone_ = height;

        // A checkpoint proves the branch to itself, the rest is resynced.
        if (chain::checkpoint::is_at(settings.checkpoints, height))
        {
            prove();
            return;
        }
    }

    if (full)
    {
        SEND(create_get_headers(state_->hash()), handle_send, _1);
        return;
    }

    finish();
}

// Verify each header against the sampled hashes and organize as proven.
void protocol_header_in_31800::collect(const headers& message,
    bool full) NOEXCEPT
{
    BC_ASSERT(stranded());

    for (const auto& ptr: message.header_ptrs)
    {
        const auto& header = *ptr;
        if (header.previous_block_hash() != previous_)
        {
            const code ec{ error::orphan_header };
            LOGR("Header [" << encode_hash(header.get_hash()) << "] from ["
                << opposite() << "] " << ec.message());
            stop(ec);
            return;
        }

        previous_ = header.get_hash();
        buffer_.push_back(ptr);
        ++height_;

        // Buffer until the next sample (at each interval and the top).
        if (!is_zero(height_ % interval_) && height_ != top_)
            continue;

        if (index_ >= samples_.size() || previous_ != samples_.at(index_))
        {
            const code ec{ error::unexpected_header };
            LOGR("Header [" << encode_hash(previous_) << ":" << height_
                << "] from [" << opposite() << "] " << ec.message());
            stop(ec);
            return;
        }

        // Organize verified segment, milestone at/under milestone.
        ++index_;
        auto height = height_ - buffer_.size();
        for (const auto& proven: buffer_)
        {
            const auto milestone = (++height <= milestone_);
            organize(proven, milestone, BIND(handle_organize, _1, _2, proven));
        }

        buffer_.clear();
        if (height_ == top_)
        {
            LOGP("Archived headers to [" << encode_hash(previous_) << ":"
                << height_ << "] from [" << opposite() << "].");

            // Resume synchronization above the proven top (state_ retained),
            // which is independent of when the organizer archives it.
            samples_.clear();
            milestone_ = zero;
            archiving_ = false;
            interval_ = max_get_headers;
            SEND(create_get_headers(previous_), handle_send, _1);
            return;
        }
    }

    if (full)
    {
        SEND(create_get_headers(previous_), handle_send, _1);
        return;
    }

    // The peer no longer presents the branch (reorganized).
    LOGP("Discarded headers from [" << opposite() << "].");

    archiving_ = false;
    state_.reset();
    buffer_.clear();
    complete();
}

// Start a branch from a stored parent (locator hit or announcement).
bool protocol_header_in_31800::restart(const hash_digest& previous) NOEXCEPT
{
    BC_ASSERT(stranded());
    state_ = archive().get_confirmed_chain_state(system_settings(), previous);
    if (!state_)
        return false;

    samples_.clear();
    milestone_ = zero;
    previous_ = previous;
    height_ = state_->height();
    interval_ = max_get_headers;
    return true;
}

// At the sample budget the interval doubles, retaining its multiples.
void protocol_header_in_31800::sample(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());
    samples_.push_back(hash);

    // Budget retains the expected header rows at the base interval.
    const auto expected = database_settings().header.expected;
    const auto budget = is_zero(expected) ? default_samples :
        ceilinged_divide(expected, max_get_headers);

    if (samples_.size() < budget)
        return;

    hashes retained{};
    retained.reserve(to_half(samples_.size()));
    const auto first = add1(height_ / interval_);

    // Sample heights are multiples of interval_ above height_ (branch start).
    for (size_t index{}; index < samples_.size(); ++index)
        if (is_zero((first + index) % two))
            retained.push_back(samples_.at(index));

    samples_ = std::move(retained);
    interval_ *= two;
}

// The branch is proven, request it again from its parent for archival.
void protocol_header_in_31800::prove() NOEXCEPT
{
    BC_ASSERT(stranded());
    top_ = state_->height();
    if (!is_zero(top_ % interval_))
        samples_.push_back(state_->hash());

    LOGP("Proven headers to [" << encode_hash(state_->hash()) << ":" << top_
        << "] from [" << opposite() << "].");

    index_ = zero;
    buffer_.clear();
    archiving_ = true;
    SEND(create_get_headers(previous_), handle_send, _1);
}

// The peer is exhausted, the branch is proven if current at minimum work.
void protocol_header_in_31800::finish() NOEXCEPT
{
    BC_ASSERT(stranded());
    const uint256_t minimum_work = system_settings().minimum_work;
    if (state_ && (state_->height() > height_) &&
        is_current_time(state_->timestamp()) &&
        (state_->cumulative_work() >= minimum_work))
    {
        prove();
        return;
    }

    LOGP("Completed headers from [" << opposite() << "].");
    state_.reset();
    complete();
}

// not stranded
void protocol_header_in_31800::handle_organize(const code& ec,
    size_t height, const chain::header::cptr& LOG_ONLY(header_ptr)) NOEXCEPT
{
    // Chaser may be stopped before protocol.
    if (stopped() ||
        ec == network::error::service_stopped ||
        ec == error::duplicate_header)
        return;

    // Assuming no store failure this is an orphan or consensus failure.
    if (ec)
    {
        if (is_zero(height))
        {
            LOGP("Header [" << encode_hash(header_ptr->get_hash())
                << "] from [" << opposite() << "] " << ec.message());
        }
        else
        {
            LOGR("Header [" << encode_hash(header_ptr->get_hash()) << ":"
                << height << "] from [" << opposite() << "] " << ec.message());
        }

        stop(ec);
        return;
    }

    LOGP("Header [" << encode_hash(header_ptr->get_hash()) << ":" << height
        << "] from [" << opposite() << "] " << ec.message());
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals peer completeness.
void protocol_header_in_31800::complete() NOEXCEPT
{
    BC_ASSERT(stranded());

    // There are no header announcements at 31800, so translate from inv.
    if (!subscribed && is_current_chain(true))
    {
        subscribed = true;
        SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
        LOGP("Subscribed to block announcements at [" << opposite() << "].");
    }
}

// Inbound (inv).
// ----------------------------------------------------------------------------
// Handle announcement by sending get_headers() if missing any announced.

bool protocol_header_in_31800::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // bip144: get_data uses witness type_id but inv does not.

    const auto& query = archive();
    for (const auto& item: message->view(type_id::block))
    {
        if (!query.is_block(item.hash))
        {
            // This is inefficient but simple and limited to protocol 31800.
            // Since subscribed is set, the response headers will be cached.
            SEND(create_get_headers(), handle_send, _1);
            return true;
        }
    }

    return true;
}

// utilities
// ----------------------------------------------------------------------------

get_headers protocol_header_in_31800::create_get_headers() const NOEXCEPT
{
    // Header sync is from the archived (strong) candidate chain.
    // Until the header tree is current the candidate chain remains empty.
    // So all channels will fully sync from the top candidate at their startup.
    const auto& query = archive();
    const auto index = get_headers::heights(query.get_top_candidate());
    return create_get_headers(query.get_candidate_hashes(index));
}

get_headers protocol_header_in_31800::create_get_headers(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_headers(hashes{ last });
}

get_headers protocol_header_in_31800::create_get_headers(
    hashes&& hashes) const NOEXCEPT
{
    if (hashes.empty())
        return {};

    if (is_one(hashes.size()))
    {
        LOGP("Request headers after [" << encode_hash(hashes.front())
            << "] from [" << opposite() << "].");
    }
    else
    {
        LOGP("Request headers (" << hashes.size()
            << ") after [" << encode_hash(hashes.front())
            << "] from [" << opposite() << "].");
    }

    return { std::move(hashes) };
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
