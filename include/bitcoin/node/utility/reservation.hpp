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
#ifndef LIBBITCOIN_NODE_RESERVATION_HPP
#define LIBBITCOIN_NODE_RESERVATION_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/performance.hpp>

namespace libbitcoin {
namespace node {

class reservations;

// Class to manage hashes during sync, thread safe.
class BCN_API reservation
  : public enable_shared_from_base<reservation>
{
public:
    typedef std::shared_ptr<reservation> ptr;
    typedef std::shared_ptr<const reservation> const_ptr;
    typedef std::vector<reservation::ptr> list;
    typedef handle0 result_handler;

    /// Construct a block reservation with the specified identifier.
    reservation(reservations& reservations, size_t slot,
        float maximum_deviation, uint32_t block_latency_seconds);

    /// Assign the reservation to a channel.
    void start();

    /// Unassign the reservation from a channel and reset.
    void stop();

    /// True if not associated with a channel.
    bool stopped() const;

    /// The sequential identifier of this reservation.
    size_t slot() const;

    /// Clear all history. Call when stopped or hashes emptied.
    void reset();

    // Rate methods.
    //-------------------------------------------------------------------------

    /// True if block import rate was more than one standard deviation low.
    bool expired() const;

    /// The point in time when the idel allowance expires.
    asio::time_point idle_limit() const;

    /// The current cached average block import rate excluding import time.
    performance rate() const;

    /// The current cached average block import rate excluding import time.
    void set_rate(performance&& rate);

    // Hash methods.
    //-------------------------------------------------------------------------

    /// True if there are currently no hashes.
    bool empty() const;

    /// The number of outstanding blocks.
    size_t size() const;

    /// Add the block hash to the reservation.
    void insert(config::checkpoint&& check);

    /// The block data request message for the outstanding block hashes.
    message::get_data request();

    // Get the height of the block hash, remove and return true if it is found.
    bool find_height_and_erase(const hash_digest& hash, size_t& out_height);

    /// Add to the blockchain, with height determined by the reservation.
    code import(blockchain::safe_chain& chain, block_const_ptr block,
        size_t height);

    /// Move half of the reservation to the specified reservation.
    bool partition(reservation::ptr minimal);

protected:
    typedef std::chrono::high_resolution_clock::time_point clock_point;

    // Accessor for testability.
    bool pending() const;

    // Accessor for testability.
    void set_pending(bool value);

    // Accessor for validating construction.
    asio::microseconds rate_window() const;

    // Isolation of side effect to enable unit testing.
    virtual clock_point now() const;

    // History methods.
    //-------------------------------------------------------------------------

    // Return rate history to startup state.
    void clear_history();

    // Update rate history to reflect an additional block of the given size.
    void update_history(size_t events, const asio::microseconds& database);

private:
    typedef struct
    {
        size_t events;
        uint64_t database;
        clock_point time;
    } history_record;

    typedef std::vector<history_record> rate_history;

    // A bidirection map is used for efficient hash and height retrieval.
    typedef boost::bimaps::bimap<
        boost::bimaps::unordered_set_of<hash_digest>,
        boost::bimaps::set_of<size_t>> hash_heights;

    // Protected by hash mutex.
    hash_heights heights_;
    mutable upgrade_mutex hash_mutex_;

    // Protected by history mutex.
    rate_history history_;
    mutable upgrade_mutex history_mutex_;

    // Thread safe.
    std::atomic<bool> stopped_;
    std::atomic<bool> pending_;
    reservations& reservations_;
    const size_t slot_;
    const float maximum_deviation_;
    const asio::microseconds rate_window_;
    bc::atomic<asio::time_point> idle_limit_;
    bc::atomic<performance> rate_;
};

} // namespace node
} // namespace libbitcoin

#endif

