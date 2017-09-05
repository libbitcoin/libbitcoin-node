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
#include <bitcoin/node/utility/reservations.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/check_list.hpp>
#include <bitcoin/node/utility/performance.hpp>
#include <bitcoin/node/utility/reservation.hpp>
#include <bitcoin/node/utility/statistics.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;

reservations::reservations(size_t minimum_peer_count, float maximum_deviation,
    uint32_t block_latency_seconds)
  : max_request_(max_get_data),
    minimum_peer_count_(minimum_peer_count),
    maximum_deviation_(maximum_deviation),
    block_latency_seconds_(block_latency_seconds)
{
}

// Rate methods.
//-----------------------------------------------------------------------------

// A statistical summary of block import rates.
// This computation is not synchronized across rows because rates are cached.
statistics reservations::rates(size_t slot, const performance& current) const
{
    // Get a copy of the list of reservation pointers.
    auto rows = table();

    // An idle row does not have sufficient history for measurement.
    const auto idle = [&](reservation::ptr row)
    {
        return row->slot() == slot ? current.idle : row->rate().idle;
    };

    // Purge idle and empty rows from the temporary table.
    rows.erase(std::remove_if(rows.begin(), rows.end(), idle), rows.end());

    const auto active_rows = rows.size();
    std::vector<double> rates(active_rows);
    const auto normal_rate = [&](reservation::ptr row)
    {
        return row->slot() == slot ? current.rate() : row->rate().rate();
    };

    // Convert to a rates table and sum.
    // BUGBUG: the rows may become idle after the above purge, skewing results.
    std::transform(rows.begin(), rows.end(), rates.begin(), normal_rate);
    const auto total = std::accumulate(rates.begin(), rates.end(), 0.0);

    // Calculate mean and sum of deviations.
    const auto mean = divide<double>(total, active_rows);
    const auto summary = [mean](double initial, double rate)
    {
        const auto difference = mean - rate;
        return initial + (difference * difference);
    };

    // Calculate the standard deviation in the rate deviations.
    auto squares = std::accumulate(rates.begin(), rates.end(), 0.0, summary);
    auto quotient = divide<double>(squares, active_rows);
    auto standard_deviation = std::sqrt(quotient);
    return{ active_rows, mean, standard_deviation };
}

// Table methods.
//-----------------------------------------------------------------------------

// protected
reservation::list reservations::table() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);

    return table_;
    ///////////////////////////////////////////////////////////////////////////
}

void reservations::remove(reservation::ptr row)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    const auto it = std::find(table_.begin(), table_.end(), row);

    if (it == table_.end())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    table_.erase(it);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

reservation::ptr reservations::get()
{
    const auto stopped = [](reservation::ptr row)
    {
        return row->stopped();
    };

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    auto it = std::find_if(table_.begin(), table_.end(), stopped);

    if (it != table_.end())
    {
        (*it)->start();
        return *it;
    }

    // Use slot table position as new slot identifier.
    // Creates new reservation unless there was already one stopped above.
    const auto row = std::make_shared<reservation>(*this, table_.size(),
        maximum_deviation_, block_latency_seconds_);

    table_.push_back(row);
    row->start();

    return row;
    ///////////////////////////////////////////////////////////////////////////
}

// Hash methods.
//-----------------------------------------------------------------------------

void reservations::pop(const hash_digest& hash, size_t height)
{
    hashes_.pop(hash, height);
}

void reservations::push(hash_digest&& hash, size_t height)
{
    hashes_.push(std::move(hash), height);
}

void reservations::enqueue(hash_digest&& hash, size_t height)
{
    hashes_.enqueue(std::move(hash), height);
}

// Call when minimal is empty.
// Take from unallocated or allocated hashes, true if minimal not empty.
void reservations::populate(reservation::ptr minimal)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();
    const auto populated = reserve(minimal) || partition(minimal);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (populated)
    {
        LOG_DEBUG(LOG_NODE)
            << "Populated " << minimal->size() << " blocks to slot ("
            << minimal->slot() << ").";
    }
}

// protected
// Return false if minimal is empty.
bool reservations::reserve(reservation::ptr minimal)
{
    if (!minimal->empty())
        return true;

    if (hashes_.empty())
        return false;

    // Consider table for incoming connections, but no less than configured.
    const auto peers = std::max(minimum_peer_count_, table_.size());
    const auto checks = hashes_.extract(peers, max_request_);

    // Order matters here.
    for (auto check: checks)
        minimal->insert(std::move(check));

    // This may become empty between insert and this test, which is okay.
    return !minimal->empty();
}

// protected
// This can cause reduction of an active reservation.
bool reservations::partition(reservation::ptr minimal)
{
    const auto maximal = find_maximal();
    return maximal && maximal != minimal && maximal->partition(minimal);
}

// protected
// The maximal row has the most block hashes reserved (prefer stopped).
reservation::ptr reservations::find_maximal()
{
    if (table_.empty())
        return nullptr;

    const auto comparer = [](reservation::ptr left, reservation::ptr right)
    {
        if (left->stopped() && !right->stopped())
            return true;

        if (right->stopped() && !left->stopped())
            return false;

        return left->size() < right->size();
    };

    return *std::max_element(table_.begin(), table_.end(), comparer);
}

// Properties.
//-----------------------------------------------------------------------------

size_t reservations::size() const
{
    return unreserved() + reserved();
}

size_t reservations::reserved() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_shared();
    auto rows = table_;
    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    const auto sum = [](size_t total, reservation::ptr row)
    {
        return total + row->size();
    };

    return std::accumulate(rows.begin(), rows.end(), size_t{0}, sum);
}

size_t reservations::unreserved() const
{
    return hashes_.size();
}

} // namespace node
} // namespace libbitcoin
