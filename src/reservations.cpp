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
#include <bitcoin/node/reservations.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/reservation.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;

// The allowed number of standard deviations below the norm.
static constexpr float rate_factor = 1.0f;

// The protocol maximum size of get data block requests.
static constexpr size_t max_block_request = 50000;

reservations::reservations(hash_queue& hashes, block_chain& chain,
    const settings& settings)
  : hashes_(hashes),
    blockchain_(chain),
    factor_(rate_factor),
    block_request_(max_block_request)
{
    initialize(settings.download_connections);
}

bool reservations::import(block::ptr block, size_t height)
{
    // Thread safe.
    return blockchain_.import(block, height);
}

// Rate methods.
//-----------------------------------------------------------------------------

// A statistical summary of block import rates.
// This computation is not synchronized across rows because rates are cached.
reservations::rate_summary reservations::rates() const
{
    // Copy row pointer table to prevent need for lock during iteration.
    auto rows = table();
    const auto idle = [](reservation::ptr row)
    {
        return row->idle();
    };

    // Remove idle rows.
    rows.erase(std::remove_if(rows.begin(), rows.end(), idle), rows.end());
    const auto row_count = rows.size();

    // Guard against division by zero.
    if (row_count == 0)
        return{ row_count, 0, 0 };

    // Optimize for trivial computation.
    if (row_count == 1)
        return{ row_count, rows.front()->rate(), 0 };

    std::vector<float> rates(row_count);
    const auto transformation = [](reservation::ptr row)
    {
        return row->rate();
    };

    // Convert to a rates table.
    std::transform(rows.begin(), rows.end(), rates.begin(), transformation);
    const float values = std::accumulate(rates.begin(), rates.end(), 0.0f);

    // Calculate mean and sum of deviations.
    const float arithmetic_mean = values / row_count;
    const auto summary = [arithmetic_mean](float initial, float rate)
    {
        const auto difference = arithmetic_mean - rate;
        return initial + (difference * difference);
    };

    // Calculate the stnadard deviation in the rate deviations.
    float squares = std::accumulate(rates.begin(), rates.end(), 0.0f, summary);
    float standard_deviation = sqrt(squares / row_count);
    return{ row_count, arithmetic_mean, standard_deviation };
}

// Table methods.
//-----------------------------------------------------------------------------

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

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();

    table_.erase(it);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

// Hash methods.
//-----------------------------------------------------------------------------

void reservations::initialize(size_t size)
{
    // Guard against overflow by capping size.

    const auto max_rows = max_size_t / block_request_;
    auto rows = std::min(max_rows, size);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock_upgrade();

    // The total number of blocks to sync.
    const auto blocks = hashes_.size();

    // Ensure that there is at least one block per row.
    rows = std::min(rows, blocks);

    if (rows == 0)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return;
    }

    // Allocate no more than 50k headers per row.
    const auto max_allocation = rows * block_request_;

    // Allocate no more than 50k headers per row.
    const auto allocation = std::min(blocks, max_allocation);

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    mutex_.unlock_upgrade_and_lock();

    auto& self = *this;
    table_.reserve(rows);

    for (auto row = 0; row < rows; ++row)
        table_.push_back(std::make_shared<reservation>(self, row, factor_));

    size_t height;
    hash_digest hash;

    // The (allocation / rows) * rows cannot exceed allocation.
    // The remainder is retained by the hash list for later reservation.
    for (size_t base = 0; base < (allocation / rows); ++base)
    {
        for (size_t row = 0; row < rows; ++row)
        {
            hashes_.pop(hash, height);
            table_[row]->insert(hash, height);
        }
    }

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    log::debug(LOG_PROTOCOL)
        << "Reserved " << allocation << " blocks to " << rows << " slots.";
}

void reservations::populate(reservation::ptr minimal)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    mutex_.lock();

    const auto reserved = reserve(minimal);

    if (!reserved)
        partition(minimal);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (reserved)
        log::debug(LOG_PROTOCOL)
            << "Reserved " << minimal->size() << " blocks to slot ("
            << minimal->slot() << ").";
}

// This can cause reduction of an active reservation.
void reservations::partition(reservation::ptr minimal)
{
    // A false ptr indicates there are no partitionable rows.
    const auto maximal = find_maximal();

    // Do not select self as it would be pontless and produce deadlock.
    if (maximal && maximal != minimal)
        maximal->partition(minimal);
}

reservation::ptr reservations::find_maximal()
{
    if (table_.empty())
        return nullptr;

    // The maximal row is that with the most block hashes reserved.
    const auto comparer = [](reservation::ptr left, reservation::ptr right)
    {
        return left->size() < right->size();
    };

    return *std::max_element(table_.begin(), table_.end(), comparer);
}

bool reservations::reserve(reservation::ptr minimal)
{
    // The unallocated blocks to sync.
    const auto size = hashes_.size();

    // Allocate no more than 50k headers to this row.
    const auto existing = minimal->size();
    const auto allocation = std::min(size, block_request_ - existing);

    size_t height;
    hash_digest hash;

    for (size_t block = 0; block < allocation; ++block)
    {
        hashes_.pop(hash, height);
        minimal->insert(hash, height);
    }

    // Accept any size here so we don't need to compensate in partitioning.
    return !minimal->empty();
}

} // namespace node
} // namespace libbitcoin
