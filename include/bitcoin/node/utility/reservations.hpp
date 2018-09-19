/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_RESERVATIONS_HPP
#define LIBBITCOIN_NODE_RESERVATIONS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>
#include <bitcoin/node/utility/check_list.hpp>
#include <bitcoin/node/utility/performance.hpp>
#include <bitcoin/node/utility/reservation.hpp>
#include <bitcoin/node/utility/statistics.hpp>

namespace libbitcoin {
namespace node {

// Class to manage a set of reservation objects during sync, thread safe.
class BCN_API reservations
{
public:
    typedef std::shared_ptr<reservations> ptr;

    /// Construct an empty table of reservations.
    reservations(size_t minimum_peer_count, float maximum_deviation,
        uint32_t block_latency_seconds);

    /// Pop header hash to back (if hash at back), verify the height.
    void pop_back(const chain::header& header, size_t height);

    /// Push header hash to back, verify the height is increasing.
    void push_back(const chain::header& header, size_t height);

    /// Push header hash to front, verify the height is decreasing.
    void push_front(hash_digest&& hash, size_t height);

    /// Get a download reservation manager.
    reservation::ptr get();

    /// Populate a starved row by taking half of the hashes from a weak row.
    void populate(reservation::ptr minimal);

    /// Check a partition for expiration.
    bool expired(reservation::const_ptr partition) const;

    /// The total number of pending block hashes.
    size_t size() const;

protected:
    // Obtain a copy of the reservations table.
    reservation::list table() const;

    // Move the maximum unreserved hashes to the specified reservation.
    bool reserve(reservation::ptr minimal);

    // Move half of the maximal reservation to the specified reservation.
    bool partition(reservation::ptr minimal);

    // Check a partition for expiration, with conditional locking.
    bool expired(reservation::const_ptr partition, bool lock) const;

    // Find the reservation with the most hashes.
    reservation::ptr find_maximal();

    // The average and standard deviation of block import rates.
    statistics rates(size_t slot, const performance& current, bool lock) const;

    // The number of hashes currently reserved.
    size_t reserved() const;

    // The number of hashes available for reservation.
    size_t unreserved() const;

private:
    ////void dump_table(size_t slot) const;

    // Thread safe.
    check_list hashes_;
    const size_t max_request_;
    const size_t minimum_peer_count_;
    const uint32_t block_latency_seconds_;
    const float maximum_deviation_;

    // Protected by mutex.
    bool initialized_;
    reservation::list table_;
    mutable upgrade_mutex mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif
