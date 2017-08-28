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

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;

reservations::reservations()
  : max_request_(max_get_data)
{
}

// Rate methods.
//-----------------------------------------------------------------------------

// A statistical summary of block import rates.
// This computation is not synchronized across rows because rates are cached.
reservations::rate_statistics reservations::rates() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_shared();

    // Craete a safe copy for computations.
    auto rows = table_;

    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    // An idle row does not have sufficient history for measurement.
    const auto idle = [](reservation::ptr row)
    {
        return row->idle();
    };

    // Purge idle and empty rows from the temporary table.
    rows.erase(std::remove_if(rows.begin(), rows.end(), idle), rows.end());

    const auto active_rows = rows.size();

    std::vector<double> rates(active_rows);
    const auto normal_rate = [](reservation::ptr row)
    {
        return row->rate().normal();
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

// Hash methods.
//-----------------------------------------------------------------------------

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
    mutex_.lock_upgrade();

    auto it = std::find_if(table_.begin(), table_.end(), stopped);

    if (it != table_.end())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        (*it)->start();
        return *it;
    }

    const auto slot = table_.size();
    auto row = std::make_shared<reservation>(*this, slot, timeout_seconds);
    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    table_.push_back(row);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return row;
}

void reservations::pop(const hash_digest& hash, size_t height)
{
    hashes_.pop(hash, height);
}

void reservations::push(hash_digest&& hash, size_t height)
{
    hashes_.enqueue(std::move(hash), height);
}

// Call when minimal is empty.
void reservations::populate(reservation::ptr minimal)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();

    // Take from unallocated or allocated hashes, true if minimal not empty.
    auto populated = reserve(minimal) || partition(minimal);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (populated)
        LOG_DEBUG(LOG_NODE)
            << "Populated " << minimal->size() << " blocks to slot ("
            << minimal->slot() << ").";
}

// private
// Return false if minimal is empty.
bool reservations::reserve(reservation::ptr minimal)
{
    if (!minimal->empty())
        return true;

    if (hashes_.empty())
        return false;

    size_t height;
    hash_digest hash;

    // Allocate hash reservation based on currently-constructed peer count.
    const auto fraction = hashes_.size() / table_.size();
    const auto allocate = range_constrain(fraction, size_t(1), max_request());

    for (size_t block = 0; block < allocate; ++block)
    {
        DEBUG_ONLY(const auto result =) hashes_.dequeue(hash, height);
        BITCOIN_ASSERT_MSG(result, "The checklist is empty.");
        minimal->insert(std::move(hash), height);
    }

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

size_t reservations::size() const
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
