/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/utility/reservation.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <boost/format.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/performance.hpp>
#include <bitcoin/node/utility/reservations.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;

// The minimum amount of block history to measure to determine window.
static constexpr size_t minimum_history = 3;

// Simple conversion factor, since we trace in microseconds.
static constexpr size_t micro_per_second = 1000 * 1000;

reservation::reservation(reservations& reservations, size_t slot,
    float maximum_deviation, uint32_t block_latency_seconds)
  : rate_({ true, 0, 0, 0 }),
    partitioned_(false),
    stopped_(true),
    pending_(false),
    reservations_(reservations),
    slot_(slot),
    maximum_deviation_(maximum_deviation),
    rate_window_(minimum_history * block_latency_seconds * micro_per_second)
{
}

void reservation::start()
{
    stopped_ = false;
    pending_ = true;
    idle_limit_.store(asio::steady_clock::now() + rate_window_);
}

void reservation::stop()
{
    stopped_ = true;
    reset();
}

bool reservation::stopped() const
{
    return stopped_;
}

size_t reservation::slot() const
{
    return slot_;
}

void reservation::reset()
{
    // No change to reserved hashes.
    set_rate({ true, 0, 0, 0 });
    clear_history();
}

// protected
bool reservation::pending() const
{
    return pending_;
}

// protected
void reservation::set_pending(bool value)
{
    pending_ = value;
}

// protected
asio::microseconds reservation::rate_window() const
{
    return rate_window_;
}

// protected
reservation::clock_point reservation::now() const
{
    return std::chrono::high_resolution_clock::now();
}

// Rate methods.
//-----------------------------------------------------------------------------

bool reservation::expired() const
{
    // Cannot expire if empty.
    if (empty())
        return false;

    const auto current = rate();

    // Cannot expire if idle unless startup limit is exceeded.
    if (current.idle)
        return asio::steady_clock::now() > idle_limit_.load();

    // Summary must be computed using same rate for slot, so pass here.
    const auto summary = reservations_.rates(slot(), current);

    // Expires if deviation exceeds norm by more than allowed.
    return current.expired(slot(), maximum_deviation_, summary);
}

performance reservation::rate() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(rate_mutex_);

    return rate_;
    ///////////////////////////////////////////////////////////////////////////
}

void reservation::set_rate(performance&& rate)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(rate_mutex_);

    rate_ = std::move(rate);
    ///////////////////////////////////////////////////////////////////////////
}

// History methods.
//-----------------------------------------------------------------------------

// protected
void reservation::clear_history()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(history_mutex_);

    history_.clear();
    ///////////////////////////////////////////////////////////////////////////
}

// protected
// It is possible to get a rate update after idling and before starting anew.
// This can reduce the average during startup of the new channel until start.
void reservation::update_history(size_t events,
    const asio::microseconds& database)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    history_mutex_.lock();

    performance rate{ false, 0, 0, 0 };
    const auto end = now();
    const auto event_start = end - database;
    const auto window_start = end - rate_window();
    const auto history_count = history_.size();

    // Remove expired entries from the head of the queue.
    for (auto it = history_.begin();
        it != history_.end() && it->time < window_start;
        it = history_.erase(it));

    const auto window_full = history_count > history_.size();
    const auto event_cost = static_cast<uint64_t>(database.count());

    // Update the event history.
    history_.push_back(history_record{ events, event_cost, event_start });

    // Do not establish a rate until there are at least three data points.
    if (history_.size() < minimum_history)
    {
        history_mutex_.unlock();
        //---------------------------------------------------------------------
        return;
    }

    // Summarize event count and database cost.
    for (const auto& record: history_)
    {
        BITCOIN_ASSERT(rate.events <= max_size_t - record.events);
        rate.events += record.events;

        BITCOIN_ASSERT(rate.discount <= max_uint64 - record.database);
        rate.discount += record.database;
    }

    // Calculate the duration of the rate window.
    auto window = window_full ? rate_window() : (end - history_.front().time);
    auto duration = std::chrono::duration_cast<asio::microseconds>(window);
    rate.window = static_cast<uint64_t>(duration.count());

    history_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Update the rate cache.
    set_rate(std::move(rate));
}

// Hash methods.
//-----------------------------------------------------------------------------

bool reservation::empty() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(hash_mutex_);

    return heights_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

size_t reservation::size() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(hash_mutex_);

    return heights_.size();
    ///////////////////////////////////////////////////////////////////////////
}

// Obtain and clear the outstanding blocks request.
message::get_data reservation::request()
{
    if (stopped())
        return{};

    // Keep outside of lock, okay if becomes empty before lock.
    if (empty())
        reservations_.populate(shared_from_this());

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    if (!pending_)
    {
        hash_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return{};
    }

    message::get_data packet;
    const auto& right = heights_.right;

    // Build get_blocks request message.
    for (auto height = right.begin(); height != right.end(); ++height)
    {
        static const auto id = message::inventory::type_id::block;
        packet.inventories().emplace_back(id, height->second);
    }

    hash_mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    pending_ = false;

    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return packet;
}

void reservation::insert(config::checkpoint&& check)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(hash_mutex_);

    pending_ = true;
    heights_.insert({ std::move(check.hash()), check.height() });
    ///////////////////////////////////////////////////////////////////////////
}

// Log only every 100th block for feedback.
inline bool enabled(size_t height)
{
    return height % 100 == 0;
}

code reservation::import(safe_chain& chain, block_const_ptr block)
{
    size_t height;
    const auto hash = block->header().hash();
    const auto encoded = encode_hash(hash);

    if (!find_height_and_erase(hash, height))
    {
        // Partitioning removes half, and the race produces these warnings.
        if (!partitioned_)
        {
            LOG_DEBUG(LOG_NODE)
                << "Ignoring unsolicited block on (" << slot() << ") ["
                << encoded << "]";
        }

        return error::success;
    }

    const auto start = now();

    //#########################################################################
    const auto ec = chain.update(block, height);
    //#########################################################################

    if (ec)
        return ec;

    // Recompute rate performance.
    auto size = block->serialized_size(message::version::level::canonical);
    auto time = std::chrono::duration_cast<asio::microseconds>(now() - start);

    // Update history data for computing peer performance standard deviation.
    update_history(size, time);

    // Log rate performance.
    if (enabled(height))
    {
        // Block #height (slot) [hash] Mbps local-cost% remaining-blocks.
        static const auto form = "Block #%06i (%02i) [%s] %07.3f %05.2f%% %i";
        const auto record = rate();
        const auto database_percentage = record.ratio() * 100;
        const auto encoded = encode_hash(block->header().hash());

        LOG_INFO(LOG_NODE)
            << boost::format(form) % height % slot() % encoded %
            performance::to_megabits_per_second(record.rate()) %
            database_percentage % reservations_.size();
    }

    return error::success;
}

bool reservation::toggle_partitioned()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    // This will cause a channel stop so the pending reservation can start.
    if (partitioned_)
    {
        hash_mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        pending_ = true;
        partitioned_ = false;
        //---------------------------------------------------------------------
        hash_mutex_.unlock();
        return true;
    }

    hash_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    return false;
}

// Give the minimal row ~ half of our hashes, return false if minimal is empty.
bool reservation::partition(reservation::ptr minimal)
{
    // This assumes that partition has been called under a table mutex.
    if (!minimal->empty())
        return true;

    // Critical Section (hash)
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    // Take half of maximal reservation, rounding up to get last entry (safe).
    // If the reservation is stopped take the full amount.
    const auto count = heights_.size();
    const auto offset = stopped_ ? count : (heights_.size() + 1u) / 2u;
    auto it = heights_.right.begin();

    hash_mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // TODO: move the range in a single command.
    for (size_t index = 0; index < offset; ++index)
    {
        minimal->heights_.right.insert(std::move(*it));
        it = heights_.right.erase(it);
    }

    hash_mutex_.unlock_and_lock_shared();
    //-------------------------------------------------------------------------
    const auto emptied = !heights_.empty();
    const auto populated = !minimal->empty();
    partitioned_ = emptied;
    minimal->pending_ = populated;

    hash_mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    if (emptied)
        reset();

    if (populated)
    {
        LOG_DEBUG(LOG_NODE)
            << "Moved [" << minimal->size() << "] blocks from slot (" << slot()
            << ") to (" << minimal->slot() << ") leaving [" << size() << "].";
    }

    return populated;
}

// private
bool reservation::find_height_and_erase(const hash_digest& hash,
    size_t& out_height)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    const auto it = heights_.left.find(hash);

    if (it == heights_.left.end())
    {
        hash_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return false;
    }

    out_height = it->second;

    hash_mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    heights_.left.erase(it);

    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return true;
}

} // namespace node
} // namespace libbitcoin
