/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/reservation.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <boost/format.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/reservations.hpp>

namespace libbitcoin {
namespace node {

using namespace std::chrono;
using namespace bc::chain;

// The allowed number of standard deviations below the norm.
// With 1 channel this multiple is irrelevant, no channels are dropped.
// With 2 channels a < 1.0 multiple will drop a channel on every test.
// With 2 channels a 1.0 multiple will fluctuate based on rounding deviations.
// With 2 channels a > 1.0 multiple will prevent all channel drops.
// With 3+ channels the multiple determines allowed deviation from the norm.
static constexpr float multiple = 1.01f;

// The minimum amount of block history to move the state from idle.
static constexpr size_t minimum_history = 3;

// TODO: move to config, since this can stall the sync.
// The minimum amount of block history to move the state from idle.
// If a new channel can't achieve 3 blocks in 15 seconds (0.2b/s) it will drop.
static constexpr size_t max_seconds_per_block = 5;

// Simple conversion factor, since we trace in micro and report in seconds.
static constexpr size_t micro_per_second = 1000 * 1000;

// The window for the rate moving average.
static const duration<int64_t, std::micro>
    rate_window(minimum_history * max_seconds_per_block * micro_per_second);

reservation::reservation(reservations& reservations, size_t slot)
  : slot_(slot),
    rate_({ true, 0, 0, 0 }),
    pending_(true),
    partitioned_(false),
    reservations_(reservations)
{
}

size_t reservation::slot() const
{
    return slot_;
}

high_resolution_clock::time_point reservation::now() const
{
    return high_resolution_clock::now();
}

// Rate methods.
//-----------------------------------------------------------------------------

// Clears rate/history but leaves hashes unchanged.
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

void reservation::set_rate(const summary_record& rate)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(rate_mutex_);

    rate_ = rate;
    ///////////////////////////////////////////////////////////////////////////
}

reservation::summary_record reservation::rate() const
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
    const auto record = rate();
    const auto normal_rate = record.normal();
    const auto statistics = reservations_.rates();
    const auto deviation = normal_rate - statistics.arithmentic_mean;
    const auto absolute_deviation = std::fabs(deviation);
    const auto allowed_deviation = multiple * statistics.standard_deviation;
    const auto outlier = absolute_deviation > allowed_deviation;
    const auto below_average = deviation < 0;
    const auto expired = below_average && outlier;

    log::debug(LOG_PROTOCOL)
        << "Statistics for slot (" << slot() << ")"
        << " adj:" << (normal_rate * micro_per_second)
        << " avg:" << (statistics.arithmentic_mean * micro_per_second)
        << " dev:" << (deviation * micro_per_second)
        << " sdv:" << (statistics.standard_deviation * micro_per_second)
        << " cnt:" << (statistics.active_count)
        << " neg:" << (below_average ? "T" : "F")
        << " out:" << (outlier ? "T" : "F")
        << " exp:" << (expired ? "T" : "F");

    return expired;
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

    // Compute import start time.
    const auto start = now() - microseconds(database);

    // The upper limit of submission time for in-window entries.
    const auto limit = start - rate_window;

    summary_record rate{ false, 0, 0, 0 };
    const auto initial_size = history_.size();

    // Remove expired entries from the head of the queue.
    for (auto it = history_.begin(); it != history_.end() && it->time < limit;
        it = history_.erase(it));
        
    // Determine if we have filled the window.
    const auto full = initial_size > history_.size();

    // Add the new entry to the tail of the queue.
    const auto cost = static_cast<uint64_t>(database.count());
    history_.push_back({ events, cost, start });

    // We can't set the rate until we have a period (2+ data points).
    if (history_.size() < minimum_history)
    {
        history_mutex_.unlock();
        //---------------------------------------------------------------------
        return;
    }

    // Calculate the rate summary.
    for (const auto& record: history_)
    {
        BITCOIN_ASSERT(rate.events <= max_size_t - record.events);
        rate.events += record.events;
        BITCOIN_ASSERT(rate.database <= max_uint64 - record.database);
        rate.database += record.database;
    }

    // If we have deleted any entries then use the full period.
    const auto window = full ? rate_window : (start - history_.front().time);
    const auto count = duration_cast<microseconds>(window).count();

    // The time difference should never be negative.
    rate.window = static_cast<uint64_t>(count);

    history_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Update the rate cache.
    set_rate(rate);

    ////log::debug(LOG_PROTOCOL)
    ////    << "Records (" << slot() << ") "
    ////    << " size: " << rate.events
    ////    << " time: " << divide<double>(rate.window, micro_per_second)
    ////    << " cost: " << divide<double>(rate.database, micro_per_second)
    ////    << " full: " << (full ? "true" : "false");
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

    auto height = heights_.right.begin();
    const auto end = heights_.right.end();
    const auto id = message::inventory_type_id::block;

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    hash_mutex_.unlock_upgrade_and_lock();

    // Build get_blocks request message.
    for (; height != end; ++height)
    {
        const message::inventory_vector inventory{ id, height->second };
        packet.inventories.emplace_back(inventory);
    }

    pending_ = false;
    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return packet;
}

void reservation::insert(const hash_digest& hash, size_t height)
{
    BITCOIN_ASSERT(height <= max_uint32);
    const auto height32 = static_cast<uint32_t>(height);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(hash_mutex_);

    pending_ = true;
    heights_.insert({ hash, height32 });
    ///////////////////////////////////////////////////////////////////////////
}

void reservation::import(block::ptr block)
{
    uint32_t height;
    const auto hash = block->header.hash();
    const auto encoded = encode_hash(hash);

    if (!find_height_and_erase(hash, height))
    {
        log::debug(LOG_PROTOCOL)
            << "Ignoring unsolicited block (" << slot() << ") ["
            << encoded << "]";
        return;
    }

    bool success;
    const auto importer = [this, &block, &height, &success]()
    {
        success = reservations_.import(block, height);
    };

    // Do the block import with timer.
    const auto cost = timer<microseconds>::duration(importer);

    if (success)
    {
        const auto size = /*block->header.transaction_count*/ 1u;
        update_rate(size, cost);

        static const auto formatter = 
            "Imported block #%06i (%02i) [%s] %06.2f %05.2f%%";

        const auto record = rate();

        log::info(LOG_PROTOCOL)
            << boost::format(formatter) % height % slot() % encoded %
            (record.total() * micro_per_second) % (record.ratio() * 100);
    }
    else
    {
        log::debug(LOG_PROTOCOL)
            << "Stopped before importing block (" << slot() << ") ["
            << encoded << "]";
    }

    if (empty())
        reservations_.populate(shared_from_this());
}

bool reservation::partitioned()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    if (partitioned_)
    {
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        hash_mutex_.unlock_upgrade_and_lock();
        partitioned_ = false;
        pending_ = true;
        hash_mutex_.unlock();
        //---------------------------------------------------------------------
        return true;
    }

    hash_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    return false;
}

// Give the minimal row ~ half of our hashes.
void reservation::partition(reservation::ptr minimal)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    const auto own_size = heights_.size();

    // Take half of the maximal reservation, rounding up to get last entry.
    BITCOIN_ASSERT(own_size < max_size_t && (own_size + 1) / 2 <= max_uint32);
    const auto offset = static_cast<uint32_t>(own_size + 1) / 2;

    // Prevent a max block request overflow.
    if (offset <= minimal->size())
    {
        hash_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return;
    }

    auto it = heights_.right.begin();

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    hash_mutex_.unlock_upgrade_and_lock();

    // TODO: move the range in a single command.
    for (size_t index = 0; index < offset; ++index)
    {
        minimal->heights_.right.insert(std::move(*it));
        it = heights_.right.erase(it);
    }

    minimal->pending_ = true;
    partitioned_ = !heights_.empty();

    hash_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    log::debug(LOG_PROTOCOL)
        << "Moved [" << minimal->size() << "] blocks from slot (" << slot()
        << ") to slot (" << minimal->slot() << ") leaving [" << size() << "].";
}

bool reservation::find_height_and_erase(const hash_digest& hash,
    uint32_t& out_height)
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
