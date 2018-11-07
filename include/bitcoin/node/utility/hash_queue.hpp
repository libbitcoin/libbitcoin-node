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
#ifndef LIBBITCOIN_NODE_HASH_QUEUE_HPP
#define LIBBITCOIN_NODE_HASH_QUEUE_HPP

#include <queue>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// A thread safe specialized inventory tracking queue.
class BCN_API hash_queue
{
public:
    /// The queue contains no entries.
    bool empty() const;

    /// Enqueue the set of hashes in order, true if previously empty.
    bool enqueue(get_data_ptr message);

    /// Remove the next entry if it matches the hash, true if matched.
    bool dequeue(const hash_digest& hash);

private:
    typedef std::queue<hash_digest> queue;

    queue queue_;
    mutable shared_mutex mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif
