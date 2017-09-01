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

reservations::reservations(size_t minimum_peer_count)
  : max_request_(max_get_data),
    minimum_peer_count_(minimum_peer_count)
{
}

// Rate methods.
//-----------------------------------------------------------------------------

// A statistical summary of block import rates.
// This computation is not synchronized across rows because rates are cached.
statistics reservations::rates(size_t slot, const performance& current) const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_shared();
    auto rows = table_;
    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    // An idle row does not have sufficient history for measurement.
    const auto idle = [&](reservation::ptr row)
    {
        return row->slot() == slot ? current.idle : row->idle();
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

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();
    table_.erase(it);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

// Create new reservation unless there is already one stopped.
reservation::ptr reservations::get_reservation(uint32_t timeout_seconds)
{
    const auto stopped = [](reservation::ptr row)
    {
        // The stopped reservation must already be idle.
        return row->stopped();
    };

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    // Require exclusivity so shared call will not find the row starting below.
    auto it = std::find_if(table_.begin(), table_.end(), stopped);

    if (it != table_.end())
    {
        (*it)->start();
        return *it;
    }

    const auto slot = table_.size();
    auto row = std::make_shared<reservation>(*this, slot, timeout_seconds);
    table_.push_back(row);
    mutex_.unlock();

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

// private
// Return false if minimal is empty.
bool reservations::reserve(reservation::ptr minimal)
{
    if (!minimal->empty())
        return true;

    if (hashes_.empty())
        return false;

    // Consider table for incoming connections, but no less than configured.
    const auto peers = std::max(minimum_peer_count_, table_.size());
    const auto checks = hashes_.extract(peers, max_request());

    // Order matters here.
    for (auto check: checks)
        minimal->insert(std::move(check));

    // This may become empty between insert and this test, which is okay.
    return !minimal->empty();
}

// private
// This can cause reduction of an active reservation.
bool reservations::partition(reservation::ptr minimal)
{
    const auto maximal = find_maximal();
    return maximal && maximal != minimal && maximal->partition(minimal);
}

// private
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

size_t reservations::max_request() const
{
    return max_request_;
}

void reservations::set_max_request(size_t value)
{
    max_request_.store(value);
}

} // namespace node
} // namespace libbitcoin
