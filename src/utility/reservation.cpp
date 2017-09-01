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

using namespace std::chrono;
using namespace bc::blockchain;
using namespace bc::chain;

// The minimum amount of block history to move the state from idle.
static constexpr size_t minimum_history = 3;

// Simple conversion factor, since we trace in microseconds.
static constexpr size_t micro_per_second = 1000 * 1000;

reservation::reservation(reservations& reservations, size_t slot,
    uint32_t block_latency_seconds)
  : rate_({ true, 0, 0, 0 }),
    pending_(true),
    partitioned_(false),
    stopped_(false),
    reservations_(reservations),
    slot_(slot),
    rate_window_(minimum_history * block_latency_seconds * micro_per_second)
{
}

reservation::~reservation()
{
    // This complicates unit testing and isn't strictly necessary.
    ////BITCOIN_ASSERT_MSG(heights_.empty(), "The reservation is not empty.");
}

size_t reservation::slot() const
{
    return slot_;
}

bool reservation::pending() const
{
    return pending_;
}

void reservation::set_pending(bool value)
{
    pending_ = value;
}

microseconds reservation::rate_window() const
{
    return rate_window_;
}

reservation::clock_point reservation::now() const
{
    return high_resolution_clock::now();
}

// Rate methods.
//-----------------------------------------------------------------------------

// Sets idle state true and clears rate/history, but leaves hashes unchanged.
void reservation::reset()
{
    set_rate({ true, 0, 0, 0 });
    clear_history();
}

// Shortcut for rate().idle call.
bool reservation::idle() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(rate_mutex_);

    return rate_.idle;
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

// Get a copy of the current rate.
performance reservation::rate() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(rate_mutex_);

    return rate_;
    ///////////////////////////////////////////////////////////////////////////
}

// Ignore idleness here, called only from an active channel, avoiding a race.
bool reservation::expired() const
{
    const auto current = rate();

    // HACK: summary must be computed using the same rate for the slot.
    return current.expired(slot(), reservations_.rates(slot(), current));
}

void reservation::clear_history()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(history_mutex_);

    history_.clear();
    ///////////////////////////////////////////////////////////////////////////
}

// It is possible to get a rate update after idling and before starting anew.
// This can reduce the average during startup of the new channel until start.
void reservation::update_rate(size_t events, const microseconds& database)
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
    history_.push_back(history_record{ events, event_cost, event_start });

    // We can't set the rate until we have a period (two or more data points).
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
    auto duration = duration_cast<microseconds>(window).count();
    rate.window = static_cast<uint64_t>(duration);

    history_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    ////LOG_DEBUG(LOG_NODE)
    ////    << "Records (" << slot() << ") "
    ////    << " size: " << rate.events
    ////    << " time: " << divide<double>(rate.window, micro_per_second)
    ////    << " cost: " << divide<double>(rate.discount, micro_per_second)
    ////    << " full: " << (full ? "true" : "false");

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

void reservation::start()
{
    stopped_ = false;
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

// Obtain and clear the outstanding blocks request.
message::get_data reservation::request(bool new_channel)
{
    message::get_data packet;

    // We are a new channel, clear history and rate data, next block starts.
    if (new_channel)
        reset();

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    if (!new_channel && !pending_)
    {
        hash_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return packet;
    }

    const auto& right = heights_.right;

    // Build get_blocks request message.
    for (auto height = right.begin(); height != right.end(); ++height)
    {
        static const auto id = message::inventory::type_id::block;
        packet.inventories().emplace_back(id, height->second);
    }

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    hash_mutex_.unlock_upgrade_and_lock();
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

void reservation::import(safe_chain& chain, block_const_ptr block,
    result_handler handler)
{
    size_t height;
    const auto hash = block->header().hash();
    const auto encoded = encode_hash(hash);

    if (!find_height_and_erase(hash, height))
    {
        LOG_DEBUG(LOG_NODE)
            << "Ignoring unsolicited block (" << slot() << ") ["
            << encoded << "]";
        handler(error::success);
        return;
    }

    const auto import_handler =
        std::bind(&reservation::handle_import,
            shared_from_base<reservation>(),
                std::placeholders::_1, block, height, now(), handler);

    //#########################################################################
    chain.update(block, height, import_handler);
    //#########################################################################
}

inline bool enabled(size_t height)
{
    return height % 10 == 0;
}

void reservation::handle_import(const code& ec, block_const_ptr block,
    size_t height, clock_point start, result_handler handler)
{
    static const auto unit_size = 1u;
    static const auto form = "Imported #%06i (%02i) [%s] %07.3f %05.2f%% %i";

    if (ec)
    {
        LOG_FATAL(LOG_NODE)
            << "Failure importing block to store, is now corrupted: "
            << ec.message();
        handler(ec);
        return;
    }

    // Recompute rate performance.
    const auto database = duration_cast<microseconds>(now() - start);
    update_rate(unit_size, database);

    // Log rate performance.
    if (enabled(height))
    {
        const auto record = rate();
        const auto hash = block->header().hash();
        const auto encoded = encode_hash(hash);
        const auto events_per_second = record.rate() * micro_per_second;
        const auto database_cost = record.ratio() * 100;

        LOG_INFO(LOG_NODE)
            << boost::format(form) % height % slot() % encoded %
            events_per_second % database_cost %  reservations_.size();
    }

    handler(error::success);
}

void reservation::populate()
{
    if (!stopped_ && empty())
    {
        reservations_.populate(shared_from_this());
        return;
    }
}

bool reservation::toggle_partitioned()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    // This will cause a channel stop so the pending reservation can start.
    if (partitioned_)
    {
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        hash_mutex_.unlock_upgrade_and_lock();
        pending_ = true;
        partitioned_ = false;
        hash_mutex_.unlock();
        //---------------------------------------------------------------------
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

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    hash_mutex_.unlock_upgrade_and_lock();

    // TODO: move the range in a single command.
    for (size_t index = 0; index < offset; ++index)
    {
        minimal->heights_.right.insert(std::move(*it));
        it = heights_.right.erase(it);
    }

    //-------------------------------------------------------------------------
    hash_mutex_.unlock_and_lock_shared();
    const auto emptied = !heights_.empty();
    const auto populated = !minimal->empty();
    partitioned_ = emptied;
    minimal->pending_ = populated;
    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (emptied)
        reset();

    if (populated)
        LOG_DEBUG(LOG_NODE)
            << "Moved [" << minimal->size() << "] blocks from slot (" << slot()
            << ") to (" << minimal->slot() << ") leaving [" << size() << "].";

    return populated;
}

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

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    hash_mutex_.unlock_upgrade_and_lock();
    heights_.left.erase(it);
    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return true;
}

} // namespace node
} // namespace libbitcoin
