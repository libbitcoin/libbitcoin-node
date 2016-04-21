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
#include <utility>
#include <boost/format.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/reservations.hpp>

namespace libbitcoin {
namespace node {

using namespace std::chrono;
using namespace bc::chain;

// The window for the rate moving average.
static const seconds rate_window(10);

// The allowed number of standard deviations below the norm.
static constexpr float deviation = 1.0f;

// Log the rate block of block download in seconds.
static constexpr size_t duration_to_seconds = 10 * 1000 * 1000;

reservation::reservation(reservations& reservations, size_t slot)
  : slot_(slot),
    idle_(true),
    rate_(0),
    adjusted_rate_(0),
    pending_(true),
    partitioned_(false),
    reservations_(reservations)
{
}

size_t reservation::slot() const
{
    return slot_;
}

system_clock::time_point reservation::current_time() const
{
    return system_clock::now();
}

// Rate methods.
//-----------------------------------------------------------------------------

void reservation::set_idle()
{
    rate_.store(0);
    adjusted_rate_.store(0);
    idle_.store(true);
}

bool reservation::idle() const
{
    return idle_.load();
}

float reservation::rate() const
{
    return adjusted_rate_.load();
}

// Ignore idleness here, called only from an active channel, avoiding a race.
bool reservation::expired() const
{
    const auto rate = rate_.load();
    const auto adjusted_rate = adjusted_rate_.load();
    const auto statistics = reservations_.rates();
    const auto deviation = adjusted_rate - statistics.arithmentic_mean;
    const auto allowed_deviation = deviation * statistics.standard_deviation;
    const auto outlier = abs(deviation) > allowed_deviation;
    const auto below_average = deviation < 0;
    const auto expired = below_average && outlier;

    ////log::debug(LOG_PROTOCOL)
    ////    << "Statistics for slot (" << slot() << ")"
    ////    << " spd:" << (rate * duration_to_seconds)
    ////    << " adj:" << (adjusted_rate * duration_to_seconds)
    ////    << " avg:" << (statistics.arithmentic_mean * duration_to_seconds)
    ////    << " dev:" << (deviation * duration_to_seconds)
    ////    << " sdv:" << (statistics.standard_deviation * duration_to_seconds)
    ////    << " cnt:" << (statistics.active_count)
    ////    << " neg:" << (below_average ? "T" : "F")
    ////    << " out:" << (outlier ? "T" : "F")
    ////    << " exp:" << (expired ? "T" : "F");

    return expired;
}

void reservation::clear_rate_history()
{
    rate_.store(0);
    adjusted_rate_.store(0);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(history_mutex_);

    history_.clear();
    ///////////////////////////////////////////////////////////////////////////
}

// Duration counts are normalized once cast to system_clock::duration.
void reservation::update_rate_history(size_t size,
    const system_clock::duration& cost)
{
    const auto now = current_time();
    const auto limit = now - rate_window;
    system_clock::duration import(0);
    float total = 0;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    history_mutex_.lock();

    const auto records = history_.size();

    // Remove expired entries from the head of the queue.
    for (auto it = history_.begin(); it != history_.end() && it->time < limit;
        it = history_.erase(it));

    const auto full = records > history_.size();

    // Add the new entry to the tail of the queue.
    history_.push_back({ size, cost, now });

    //------------------------------------------------------------------------
    history_mutex_.unlock_and_lock_shared();

    // Calculate the rate summary.
    for (const auto& record: history_)
    {
        // TODO: guard against overflows.
        total += record.size;
        import += record.import;
    }

    // If we have deleted any entries then use the full period.
    const auto period = full ? rate_window : now - history_.front().time;

    history_mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    const auto import_count = import.count();
    const auto period_count = period.count();

    auto rate = total / period_count;
    auto adjusted_rate = total / (period_count - import_count);

    if (rate != rate)
        rate = 0;

    if (adjusted_rate != adjusted_rate)
        adjusted_rate = 0;

    rate_.store(rate);
    adjusted_rate_.store(adjusted_rate);

    ////log::debug(LOG_PROTOCOL)
    ////    << "Records (" << slot() << ") "
    ////    << " size: " << size
    ////    << " cost: " << cost.count()
    ////    << " recs: " << records
    ////    << " totl: " << total
    ////    << " time: " << period_count
    ////    << " disc: " << import_count
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
message::get_data reservation::request(bool reset)
{
    message::get_data packet;

    if (reset)
        clear_rate_history();

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    hash_mutex_.lock_upgrade();

    if (!reset && !pending_)
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

    // This prevents inclusion of a reservation rate before the first block.
    // Expiration does not consider idelness so delay does not prevent closure.
    idle_.store(false);

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

    const auto cost = timer<microseconds>::duration(importer);

    if (success)
    {
        static const auto formatter = 
            "Imported block #%06i (%02i) [%s] %07.3f %-01.2f%%";

        const auto slower_rate = rate_.load();
        const auto faster_rate = adjusted_rate_.load();

        // Convert rates to time per block based on common block count.
        const auto lesser_time = 1 / faster_rate;
        const auto greater_time = 1 / slower_rate;

        // Calculate the percentage of total time spent in the database.
        const auto factor = (greater_time - lesser_time) / greater_time;
        const auto percent = factor != factor ? 0.0f : 100 * factor;

        // Convert the total time to seconds.
        auto rate = slower_rate * duration_to_seconds;
        rate = rate != rate ? 0.0f : rate;

        log::info(LOG_PROTOCOL)
            << boost::format(formatter) % height % slot() % encoded % rate %
            percent;

        const auto size = /*block->header.transaction_count*/ 1u;
        update_rate_history(size, cost);
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
