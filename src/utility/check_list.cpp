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
#include <bitcoin/node/utility/check_list.hpp>

#include <cstddef>
#include <utility>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace node {

bool check_list::empty() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    return checks_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

size_t check_list::size() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    return checks_.size();
    ///////////////////////////////////////////////////////////////////////////
}

void check_list::pop(const hash_digest& hash, size_t height)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty() || checks_.front().hash() != hash)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return;
    }

    if (checks_.front().height() != height)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "popped invalid height for hash");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    checks_.pop_back();
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

void check_list::enqueue(hash_digest&& hash, size_t height)
{
    BITCOIN_ASSERT_MSG(height != 0, "pushed genesis height for download");

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.front().height() > height)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "pushed height out of order");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.emplace_back(std::move(hash), height);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

bool check_list::dequeue(hash_digest& out_hash, size_t& out_height)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return false;
    }

    const auto front = checks_.front();
    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.pop_front();
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    out_hash = std::move(front.hash());
    out_height = front.height();
    return true;
}

} // namespace node
} // namespace libbitcoin
