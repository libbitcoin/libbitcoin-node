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
#ifndef LIBBITCOIN_NODE_RESERVATIONS_HPP
#define LIBBITCOIN_NODE_RESERVATIONS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>
#include <bitcoin/node/utility/check_list.hpp>
#include <bitcoin/node/utility/reservation.hpp>

namespace libbitcoin {
namespace node {

// Class to manage a set of reservation objects during sync, thread safe.
class BCN_API reservations
{
public:
    typedef struct
    {
        size_t active_count;
        double arithmentic_mean;
        double standard_deviation;
    } rate_statistics;

    typedef std::shared_ptr<reservations> ptr;

    /// Construct an empty table of reservations.
    reservations();

    /// The average and standard deviation of block import rates.
    rate_statistics rates() const;

    /// Get a download reservation manager.
    reservation::ptr get_reservation(uint32_t timeout_seconds);

    /// Pop reorganized header hash (if hash at top), verify the height.
    void pop(const hash_digest& hash, size_t height);

    /// Push organized header hash, verify the height is increasing.
    void push(hash_digest&& hash, size_t height);

    /// Import the given block to the blockchain at the specified height.
    bool import(block_const_ptr block, size_t height);

    /// Populate a starved row by taking half of the hashes from a weak row.
    void populate(reservation::ptr minimal);

    /// Remove the row from the reservation table if found.
    void remove(reservation::ptr row);

    /// The max size of a block request.
    size_t max_request() const;

    /// Set the max size of a block request (defaults to 50000).
    void set_max_request(size_t value);

private:
    // Find the reservation with the most hashes.
    reservation::ptr find_maximal();

    // Move half of the maximal reservation to the specified reservation.
    bool partition(reservation::ptr minimal);

    // Move the maximum unreserved hashes to the specified reservation.
    bool reserve(reservation::ptr minimal);

    // Thread safe.
    check_list hashes_;
    std::atomic<size_t> max_request_;

    // Protected by mutex.
    reservation::list table_;
    mutable upgrade_mutex mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif

