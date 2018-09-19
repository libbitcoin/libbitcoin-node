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
#include <bitcoin/node/utility/check_list.hpp>

#include <cstddef>
#include <iterator>
#include <list>
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

void check_list::push_back(hash_digest&& hash, size_t height)
{
    BITCOIN_ASSERT_MSG(height != 0, "pushed genesis height for download");

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (!checks_.empty() && checks_.back().height() >= height)
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

void check_list::pop_back(const hash_digest& hash, size_t height)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty() || checks_.back().hash() != hash)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        ////BITCOIN_ASSERT_MSG(false, "popped from empty list");
        return;
    }

    if (checks_.back().height() != height)
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

void check_list::push_front(hash_digest&& hash, size_t height)
{
    BITCOIN_ASSERT_MSG(height != 0, "enqueued genesis height for download");

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (!checks_.empty() && height >= checks_.front().height())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "enqueued height out of order");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.emplace_front(std::move(hash), height);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

config::checkpoint check_list::pop_front()
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "dequeued from empty list");
        return {};
    }

    const auto check = checks_.front();

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.pop_front();

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return check;
}

// protected
void check_list::advance(checks::iterator& it, size_t step)
{
    for (size_t i = 0; it != checks_.end() && i < step;
        it = std::next(it), ++i);
}

check_list::checks check_list::extract(size_t divisor, size_t limit)
{
    if (divisor == 0 || limit == 0)
        return {};

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    // Guard against empty initial list (loop safety).
    if (checks_.empty())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return {};
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    checks result;
    const auto step = divisor - 1u;

    for (auto it = checks_.begin();
        it != checks_.end() && result.size() < limit; advance(it, step))
    {
        result.push_front(*it);
        it = checks_.erase(it);
    }

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return result;
}

} // namespace node
} // namespace libbitcoin
