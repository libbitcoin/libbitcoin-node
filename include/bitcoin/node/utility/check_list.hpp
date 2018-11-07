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
#ifndef LIBBITCOIN_NODE_CHECK_LIST_HPP
#define LIBBITCOIN_NODE_CHECK_LIST_HPP

#include <cstddef>
#include <list>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// A thread safe checkpoint deque.
class BCN_API check_list
{
public:
    typedef std::list<config::checkpoint> checks;

    /// The queue contains no checkpoints.
    bool empty() const;

    /// The number of checkpoints in the queue.
    size_t size() const;

    /// Push an entry at back, verify the height is increasing.
    void push_back(hash_digest&& hash, size_t height);

    /// Pop an entry if exists at back, verify the height.
    void pop_back(const hash_digest& hash, size_t height);

    /// Push an entry at front, verify the height is decreasing.
    void push_front(hash_digest&& hash, size_t height);

    /// Pop an entry from front, null/zero if empty.
    config::checkpoint pop_front();

    /// Remove and return a fraction of the list, up to a limit.
    checks extract(size_t divisor, size_t limit);

protected:
    // Overflow safe iteration step.
    void advance(checks::iterator& it, size_t step);

private:
    checks checks_;
    mutable shared_mutex mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif
