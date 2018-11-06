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
#include <bitcoin/node/utility/hash_queue.hpp>

#include <utility>
#include <boost/bimap/support/lambda.hpp>
#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

bool hash_queue::empty() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    return queue_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

// Enqueue the block inventory behind the preceding block inventory.
bool hash_queue::enqueue(get_data_ptr message)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    const auto was_empty = queue_.empty();

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    for (const auto& inventory: message->inventories())
        queue_.push(inventory.hash());

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return was_empty;
}

bool hash_queue::dequeue(const hash_digest& hash)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (!queue_.empty() && queue_.front() == hash)
    {
        mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        queue_.pop();
        //---------------------------------------------------------------------
        mutex_.unlock();
        return true;
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    return false;
}

} // namespace node
} // namespace libbitcoin
