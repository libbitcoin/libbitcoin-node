/////**
//// * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
//// *
//// * This file is part of libbitcoin.
//// *
//// * This program is free software: you can redistribute it and/or modify
//// * it under the terms of the GNU Affero General Public License as published by
//// * the Free Software Foundation, either version 3 of the License, or
//// * (at your option) any later version.
//// *
//// * This program is distributed in the hope that it will be useful,
//// * but WITHOUT ANY WARRANTY; without even the implied warranty of
//// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// * GNU Affero General Public License for more details.
//// *
//// * You should have received a copy of the GNU Affero General Public License
//// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//// */
////#include <bitcoin/node/utility/reservations.hpp>
////
////#include <algorithm>
////#include <cmath>
////#include <cstddef>
////#include <memory>
////#include <numeric>
////#include <utility>
////#include <vector>
////#include <bitcoin/bitcoin.hpp>
////#include <bitcoin/node/define.hpp>
////#include <bitcoin/node/utility/check_list.hpp>
////#include <bitcoin/node/utility/performance.hpp>
////#include <bitcoin/node/utility/reservation.hpp>
////
////namespace libbitcoin {
////namespace node {
////
////using namespace bc::blockchain;
////using namespace bc::chain;
////
////reservations::reservations(check_list& hashes, fast_chain& chain,
////    const settings& settings)
////  : hashes_(hashes),
////    max_request_(max_get_data),
////    timeout_(settings.sync_timeout_seconds),
////    chain_(chain)
////{
////    initialize(std::min(settings.sync_peers, 3u));
////}
////
////bool reservations::start()
////{
////    return false;//// chain_.begin_insert();
////}
////
////bool reservations::import(block_const_ptr block, size_t height)
////{
////    //#########################################################################
////    return false;//// chain_.insert(block, height);
////    //#########################################################################
////}
////
////bool reservations::stop()
////{
////    return false;//// chain_.end_insert();
////}
////
////// Rate methods.
//////-----------------------------------------------------------------------------
////
////// A statistical summary of block import rates.
////// This computation is not synchronized across rows because rates are cached.
////reservations::rate_statistics reservations::rates() const
////{
////    // Copy row pointer table to prevent need for lock during iteration.
////    auto rows = table();
////    const auto idle = [](reservation::ptr row)
////    {
////        return row->idle();
////    };
////
////    // Remove idle rows from the table.
////    rows.erase(std::remove_if(rows.begin(), rows.end(), idle), rows.end());
////    const auto active_rows = rows.size();
////
////    std::vector<double> rates(active_rows);
////    const auto normal_rate = [](reservation::ptr row)
////    {
////        return row->rate().normal();
////    };
////
////    // Convert to a rates table and sum.
////    std::transform(rows.begin(), rows.end(), rates.begin(), normal_rate);
////    const auto total = std::accumulate(rates.begin(), rates.end(), 0.0);
////
////    // Calculate mean and sum of deviations.
////    const auto mean = divide<double>(total, active_rows);
////    const auto summary = [mean](double initial, double rate)
////    {
////        const auto difference = mean - rate;
////        return initial + (difference * difference);
////    };
////
////    // Calculate the standard deviation in the rate deviations.
////    auto squares = std::accumulate(rates.begin(), rates.end(), 0.0, summary);
////    auto quotient = divide<double>(squares, active_rows);
////    auto standard_deviation = std::sqrt(quotient);
////    return{ active_rows, mean, standard_deviation };
////}
////
////// Table methods.
//////-----------------------------------------------------------------------------
////
////reservation::list reservations::table() const
////{
////    // Critical Section
////    ///////////////////////////////////////////////////////////////////////////
////    shared_lock lock(mutex_);
////
////    return table_;
////    ///////////////////////////////////////////////////////////////////////////
////}
////
////void reservations::remove(reservation::ptr row)
////{
////    // Critical Section
////    ///////////////////////////////////////////////////////////////////////////
////    mutex_.lock_upgrade();
////
////    const auto it = std::find(table_.begin(), table_.end(), row);
////
////    if (it == table_.end())
////    {
////        mutex_.unlock_upgrade();
////        //---------------------------------------------------------------------
////        return;
////    }
////
////    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
////    mutex_.unlock_upgrade_and_lock();
////    table_.erase(it);
////    mutex_.unlock();
////    ///////////////////////////////////////////////////////////////////////////
////}
////
////// Hash methods.
//////-----------------------------------------------------------------------------
////
////// No critical section because this is private to the constructor.
////void reservations::initialize(size_t connections)
////{
////    // Guard against overflow by capping size.
////    const size_t max_rows = max_size_t / max_request();
////    auto rows = std::min(max_rows, connections);
////
////    // Ensure that there is at least one block per row.
////    const auto blocks = hashes_.size();
////    rows = std::min(rows, blocks);
////
////    // Guard against division by zero.
////    if (rows == 0)
////        return;
////
////    table_.reserve(rows);
////
////    // Allocate up to 50k headers per row.
////    const auto max_allocation = rows * max_request();
////    const auto allocation = std::min(blocks, max_allocation);
////
////    for (size_t row = 0; row < rows; ++row)
////        table_.push_back(std::make_shared<reservation>(*this, row, timeout_));
////
////    size_t height;
////    hash_digest hash;
////
////    // The (allocation / rows) * rows cannot exceed allocation.
////    // The remainder is retained by the hash list for later reservation.
////    for (size_t base = 0; base < (allocation / rows); ++base)
////    {
////        for (size_t row = 0; row < rows; ++row)
////        {
////            DEBUG_ONLY(const auto result =) hashes_.dequeue(hash, height);
////            BITCOIN_ASSERT_MSG(result, "The checklist is empty.");
////            table_[row]->insert(std::move(hash), height);
////        }
////    }
////
////    LOG_DEBUG(LOG_NODE)
////        << "Reserved " << allocation << " blocks to " << rows << " slots.";
////}
////
////// Call when minimal is empty.
////bool reservations::populate(reservation::ptr minimal)
////{
////    // Critical Section
////    ///////////////////////////////////////////////////////////////////////////
////    mutex_.lock();
////
////    // Take from unallocated or allocated hashes, true if minimal not empty.
////    const auto populated = reserve(minimal) || partition(minimal);
////
////    mutex_.unlock();
////    ///////////////////////////////////////////////////////////////////////////
////
////    if (populated)
////        LOG_DEBUG(LOG_NODE)
////            << "Populated " << minimal->size() << " blocks to slot ("
////            << minimal->slot() << ").";
////
////    return populated;
////}
////
////// This can cause reduction of an active reservation.
////bool reservations::partition(reservation::ptr minimal)
////{
////    const auto maximal = find_maximal();
////    return maximal && maximal != minimal && maximal->partition(minimal);
////}
////
////reservation::ptr reservations::find_maximal()
////{
////    if (table_.empty())
////        return nullptr;
////
////    // The maximal row is that with the most block hashes reserved.
////    const auto comparer = [](reservation::ptr left, reservation::ptr right)
////    {
////        return left->size() < right->size();
////    };
////
////    return *std::max_element(table_.begin(), table_.end(), comparer);
////}
////
////// Return false if minimal is empty.
////bool reservations::reserve(reservation::ptr minimal)
////{
////    if (!minimal->empty())
////        return true;
////
////    const auto allocation = std::min(hashes_.size(), max_request());
////
////    size_t height;
////    hash_digest hash;
////
////    for (size_t block = 0; block < allocation; ++block)
////    {
////        DEBUG_ONLY(const auto result =) hashes_.dequeue(hash, height);
////        BITCOIN_ASSERT_MSG(result, "The checklist is empty.");
////        minimal->insert(std::move(hash), height);
////    }
////
////    // This may become empty between insert and this test, which is okay.
////    return !minimal->empty();
////}
////
////// Exposed for test to be able to control the request size.
////size_t reservations::max_request() const
////{
////    return max_request_;
////}
////
////// Exposed for test to be able to control the request size.
////void reservations::set_max_request(size_t value)
////{
////    max_request_.store(value);
////}
////
////} // namespace node
////} // namespace libbitcoin
