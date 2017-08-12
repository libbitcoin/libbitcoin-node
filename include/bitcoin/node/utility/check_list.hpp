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
#ifndef LIBBITCOIN_NODE_CHECK_LIST_HPP
#define LIBBITCOIN_NODE_CHECK_LIST_HPP

#include <cstddef>
#include <list>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// A thread safe checkpoint queue.
class BCN_API check_list
{
public:
    /// The queue contains no checkpoints.
    bool empty() const;

    /// The number of checkpoints in the queue.
    size_t size() const;

    /// Pop an entry if exists at top, verify the height.
    void pop(const hash_digest& hash, size_t height);

    // TODO: rename to push.
    /// Push an entry, verify the height is increasing.
    void enqueue(hash_digest&& hash, size_t height);

    // TODO: return checkpoint.
    /// Dequeue an entry.
    bool dequeue(hash_digest& out_hash, size_t& out_height);

private:

    std::list<config::checkpoint> checks_;
    mutable shared_mutex mutex_;
};

} // namespace node
} // namespace libbitcoin

#endif
