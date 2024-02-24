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
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>

#include <chrono>
#include <functional>
#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/error.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Performance polling.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::handle_performance_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected channel strand");

    if (stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        LOGF("Performance timer error, " << ec.message());
        stop(ec);
        return;
    }

    // Compute rate in bytes per second.
    const auto now = steady_clock::now();
    const auto gap = std::chrono::duration_cast<seconds>(now - start_).count();
    const auto rate = floored_divide(bytes_, gap);
    LOGN("Rate ["
        << identifier() << "] ("
        << bytes_ << "/"
        << gap << " = "
        << rate << ").");

    // Reset counters and log rate.
    bytes_ = zero;
    start_ = now;
    ////log.fire(event_block, rate);

    // Bounces to network strand, performs work, then calls handler.
    // Channel will continue to process blocks while this call excecutes on the
    // network strand. Timer will not be restarted until this call completes.
    performance(identifier(), rate, BIND1(handle_performance, ec));
}

void protocol_block_in_31800::handle_performance(const code& ec) NOEXCEPT
{
    POST1(do_handle_performance, ec);
}

void protocol_block_in_31800::do_handle_performance(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "expected network strand");

    if (stopped())
        return;

    // stalled_channel or slow_channel
    if (ec)
    {
        LOGF("Performance action, " << ec.message());
        stop(ec);
        return;
    };

    performance_timer_->start(BIND1(handle_performance_timer, _1));
}

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");

    if (started())
        return;

    const auto& query = archive();
    const auto top = query.get_top_candidate();
    top_ = { query.get_header_key(query.to_candidate(top)), top };

    ////if (report_performance_)
    ////{
    ////    start_ = steady_clock::now();
    ////    performance_timer_->start(BIND1(handle_performance_timer, _1));
    ////}

    // There is one persistent common inventory subscription.
    SUBSCRIBE_CHANNEL2(inventory, handle_receive_inventory, _1, _2);
    SEND1(create_get_inventory(), handle_send, _1);
    protocol::start();
}

void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");
    ////performance_timer_->stop();
    protocol::stopping(ec);
}

// Inbound (blocks).
// ----------------------------------------------------------------------------

// Receive inventory and send get_data for all blocks that are not found.
bool protocol_block_in_31800::handle_receive_inventory(const code& ec,
    const inventory::cptr& message) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");
    constexpr auto block_id = inventory::type_id::block;

    if (stopped(ec))
        return false;

    LOGP("Received (" << message->count(block_id) << ") block inventory from ["
        << authority() << "].");

    const auto getter = create_get_data(*message);

    // If getter is empty it may be only because we have them all, so iterate.
    if (getter.items.empty())
    {
        // If the original request was maximal, we assume there are more.
        if (message->items.size() == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (empty maximal).");
            SEND1(create_get_inventory(message->items.back().hash),
                handle_send, _1);
        }

        return true;
    }

    LOGP("Requesting (" << getter.items.size() << ") blocks from ["
        << authority() << "].");

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto tracker = std::make_shared<track>(track
    {
        getter.items.size(),
        getter.items.back().hash,
        to_hashes(getter)
    });
    BC_POP_WARNING()

    // TODO: these should be limited in quantity for DOS protection.
    // There is one block subscription for each received unexhausted inventory.
    SUBSCRIBE_CHANNEL3(block, handle_receive_block, _1, _2, tracker);
    SEND1(getter, handle_send, _1);
    return true;
}

// Process block responses in order as dictated by tracker.
bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message, const track_ptr& tracker) NOEXCEPT
{
    BC_ASSERT_MSG(stranded(), "protocol_block_in_31800");

    if (stopped(ec))
        return false;

    if (tracker->hashes.empty())
    {
        LOGF("Exhausted block tracker.");
        return false;
    }

    // Alias.
    const auto& block_ptr = message->block_ptr;

    // Unrequested block, may not have been announced via inventory.
    if (tracker->hashes.back() != block_ptr->hash())
        return true;

    // Out of order or invalid.
    if (block_ptr->header().previous_block_hash() != top_.hash())
    {
        LOGP("Orphan block [" << encode_hash(block_ptr->hash())
            << "] from [" << authority() << "].");
        return false;
    }

    // Add block at next height.
    const auto height = add1(top_.height());

    // Asynchronous organization serves all channels.
    // A job backlog will occur when organize is slower than download.
    // This should not be a material issue given lack of validation here.
    organize(block_ptr, BIND3(handle_organize, _1, height, block_ptr));

    // Set the new top and continue. Organize error will stop the channel.
    top_ = { block_ptr->hash(), height };

    ////// Accumulate byte count.
    ////bytes_ += message->cached_size;

    // Order is reversed, so next is at back.
    tracker->hashes.pop_back();

    // Handle completion of the inventory block subset.
    if (tracker->hashes.empty())
    {
        // Protocol presumes max_get_blocks unless complete.
        if (tracker->announced == max_get_blocks)
        {
            LOGP("Get inventory [" << authority() << "] (exhausted maximal).");
            SEND1(create_get_inventory(tracker->last), handle_send, _1);
        }
        else
        {
            // Completeness stalls if on 500 as empty message is ambiguous.
            // This is ok, since complete is not used for anything essential.
            complete();
        }
    }

    // Release subscription if exhausted.
    // handle_receive_inventory will restart inventory iteration.
    return !tracker->hashes.empty();
}

// This could be the end of a catch-up sequence, or a singleton announcement.
// The distinction is ultimately arbitrary, but this signals initial currency.
void protocol_block_in_31800::complete() NOEXCEPT
{
    LOGN("Blocks from [" << authority() << "] complete at ("
        << top_.height() << ").");
}

void protocol_block_in_31800::handle_organize(const code& ec, size_t height,
    const chain::block::cptr& block_ptr) NOEXCEPT
{
    if (ec == network::error::service_stopped)
        return;

    if (!ec || ec == error::duplicate_block)
    {
        LOGP("Block [" << encode_hash(block_ptr->hash())
            << "] at (" << height << ") from [" << authority() << "] "
            << ec.message());
        return;
    }

    // Assuming no store failure this is a consensus failure.
    LOGR("Block [" << encode_hash(block_ptr->hash())
        << "] at (" << height << ") from [" << authority() << "] "
        << ec.message());

    stop(ec);
}


// private
// ----------------------------------------------------------------------------

get_blocks protocol_block_in_31800::create_get_inventory() const NOEXCEPT
{
    return create_get_inventory(archive().get_candidate_hashes(
        get_blocks::heights(archive().get_top_candidate())));
}

get_blocks protocol_block_in_31800::create_get_inventory(
    const hash_digest& last) const NOEXCEPT
{
    return create_get_inventory(hashes{ last });
}

get_blocks protocol_block_in_31800::create_get_inventory(
    hashes&& hashes) const NOEXCEPT
{
    if (!hashes.empty())
    {
        LOGP("Request blocks after [" << encode_hash(hashes.front())
            << "] from [" << authority() << "].");
    }

    return { std::move(hashes) };
}

get_data protocol_block_in_31800::create_get_data(
    const inventory& message) const NOEXCEPT
{
    get_data getter{};
    getter.items.reserve(message.count(type_id::block));

    // clang emplace_back bug (no matching constructor), using push_back.
    // bip144: get_data uses witness constant but inventory does not.
    for (const auto& item: message.items)
        if ((item.type == type_id::block) && !archive().is_block(item.hash))
            getter.items.push_back({ block_type_, item.hash });

    getter.items.shrink_to_fit();
    return getter;
}

// static
hashes protocol_block_in_31800::to_hashes(const get_data& getter) NOEXCEPT
{
    hashes out{};
    out.resize(getter.items.size());

    // Order reversed for individual erase performance (using pop_back).
    std::transform(getter.items.rbegin(), getter.items.rend(), out.begin(),
        [](const auto& item) NOEXCEPT
        {
            return item.hash;
        });

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
