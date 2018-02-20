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
    block_latency_seconds_(block_latency_seconds),
    maximum_deviation_(maximum_deviation),
    initialized_(false)
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

    // Guarantee the minimal row set (first run only).
    for (size_t index = table_.size(); index < minimum_peer_count_; ++index)
        table_.push_back(std::make_shared<reservation>(*this, index,
            maximum_deviation_, block_latency_seconds_));

    // Find the first row that is not stopped.
    auto it = std::find_if(table_.begin(), table_.end(), stopped);

    if (it != table_.end())
    {
        (*it)->start();
        return *it;
    }

    // This allows for the addition of incoming connections.
    const auto row = std::make_shared<reservation>(*this, table_.size(),
        maximum_deviation_, block_latency_seconds_);
    table_.push_back(row);
    row->start();

    return row;
    ///////////////////////////////////////////////////////////////////////////
}

// Hash methods.
//-----------------------------------------------------------------------------

void reservations::pop_back(const chain::header& header, size_t height)
{
    hashes_.pop_back(header.hash(), height);
}

void reservations::push_back(const chain::header& header, size_t height)
{
    if (!header.validation.populated)
        hashes_.push_back(header.hash(), height);
}

void reservations::push_front(hash_digest&& hash, size_t height)
{
    hashes_.push_front(std::move(hash), height);
}

// Call when minimal is empty.
// Take from unallocated or allocated hashes, true if minimal not empty.
bool reservations::populate(reservation::ptr minimal)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    return reserve(minimal) || partition(minimal);
    ///////////////////////////////////////////////////////////////////////////
}

// protected
bool reservations::reserve(reservation::ptr minimal)
{
    // Intitialize the row set as late as possible.
    if (!initialized_)
    {
        initialized_ = true;
        const auto count = max_request_ * minimum_peer_count_;
        const auto checks = std::min(count, hashes_.size());

        // Balance set size and block heights across the minimal row set.
        for (size_t check = 0; check < checks; ++check)
            table_[check % minimum_peer_count_]->insert(hashes_.pop_front());
    }

    if (!minimal->empty())
        return true;

    // Obtain own fraction of whatever hashes remain unreserved.
    const auto checks = hashes_.extract(table_.size(), max_request_);
    const auto reserved = !checks.empty();

    // Order matters here.
    for (auto check: checks)
        minimal->insert(std::move(check));

    if (reserved)
    {
        LOG_DEBUG(LOG_NODE)
            << "Reserved " << minimal->size() << " blocks to slot ("
            << minimal->slot() << ").";
    }

    return reserved;
}

// protected
bool reservations::partition(reservation::ptr minimal)
{
    if (!minimal->empty())
        return true;

    const auto maximal = find_maximal();

    if (!maximal || maximal == minimal)
        return false;

    // This causes reduction of an active reservation, requiring a stop.
    const auto partitioned = maximal->partition(minimal);

    if (partitioned)
    {
        LOG_DEBUG(LOG_NODE)
            << "Partitioned " << minimal->size() << " blocks from slot ("
            << maximal->slot() << ") to slot (" << minimal->slot() << ").";
    }

    return partitioned;
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

    const auto max = *std::max_element(table_.begin(), table_.end(), comparer);

    // If down to one hash and the owner is not expired, do not partition.
    return max->size() == 1 && !max->expired() ? nullptr : max;
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
