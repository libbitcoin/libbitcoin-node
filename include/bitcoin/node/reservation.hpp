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
#ifndef LIBBITCOIN_NODE_RESERVATION_HPP
#define LIBBITCOIN_NODE_RESERVATION_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class reservations;

// Thread safe.
class BCN_API reservation
  : public enable_shared_from_base<reservation>
{
public:
    typedef std::shared_ptr<reservation> ptr;
    typedef std::vector<reservation::ptr> list;

    /// Construct a block reservation with the specified identifier.
    reservation(reservations& reservations, size_t slot, float rate_factor);

    /// The sequential identifier of this reservation.
    size_t slot() const;

    /// True if there are any outstanding blocks.
    bool empty() const;

    /// The number of outstanding blocks.
    size_t size() const;

    /// True if block import rate was more than one standard deviation low.
    bool expired() const;

    /// Sets the idle state to true. Call when channel is stopped.
    void reservation::set_idle();

    /// True if the reservation is not applied to a channel.
    bool idle() const;

    /// The current cached average block import rate excluding import time.
    float rate() const;

    /// The block data request message for the outstanding block hashes.
    /// Set reset if the preceding request was unsuccessful or discarded.
    message::get_data request(bool reset);

    /// Add the block hash to the reservation.
    void insert(const hash_digest& hash, size_t height);

    /// Add to the blockchain, with height determined by the reservation.
    void import(chain::block::ptr block);

    /// Determine if the reservation was partitioned and reset partition flag.
    bool partitioned();

    /// Move half of the reservation to the specified reservation.
    void partition(reservation::ptr minimal);

protected:

    // Isolation of side effect to enable unit testing.
    virtual std::chrono::system_clock::time_point current_time() const;

private:
    typedef struct
    {
        size_t size;
        std::chrono::system_clock::duration import;
        std::chrono::system_clock::time_point time;
    } import_record;
    typedef std::vector<import_record> rate_history;

    // A bidirection map is used for efficient hash and height retrieval.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<hash_digest>,
        boost::bimaps::set_of<uint32_t >> hash_heights;

    // Return rate history to startup state.
    void clear_rate_history();

    // Get the height of the block hash, remove and return true if it is found.
    bool find_height_and_erase(const hash_digest& hash, uint32_t& out_height);

    // Update rate history to reflect an additional block of the given size.
    void update_rate_history(size_t size,
        const std::chrono::system_clock::duration& cost);

    // The sequential identifier of the reservation instance.
    const size_t slot_;

    // The allowable amount of standard deviation from the norm.
    const float factor_;

    // Thread safe.
    std::atomic<bool> idle_;
    std::atomic<float> rate_;
    std::atomic<float> adjusted_rate_;
    reservations& reservations_;

    // Protected by history mutex.
    rate_history history_;
    mutable upgrade_mutex history_mutex_;

    // Protected by hash mutex.
    bool pending_;
    bool partitioned_;
    hash_heights heights_;
    mutable upgrade_mutex hash_mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif

