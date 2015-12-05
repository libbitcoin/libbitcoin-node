/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_P2P_NODE_HPP
#define LIBBITCOIN_NODE_P2P_NODE_HPP

#include <cstdint>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

/// A full node on the Bitcoin P2P network.
class BCN_API p2p_node
  : public network::p2p
{
public:
    static const configuration defaults;

    /// Construct the full node.
    p2p_node(const configuration& configuration);

    /// Destruct the full node.
    ~p2p_node();

    /// Invoke startup and seeding sequence, call from constructing thread.
    void start(result_handler handler) override;

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    void run(result_handler handler) override;

    /// Non-blocking call to coalesce all work, start may be reinvoked after.
    /// Handler returns the result of file save operations.
    void stop(result_handler handler) override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    void close() override;

private:
    void handle_blockchain_start(const code& ec, result_handler handler);
    void handle_fetch_height(const code& ec, uint64_t height,
        result_handler handler);
    void handle_fetch_header(const code& ec, const chain::header& block_header,
        size_t block_height, result_handler handler);
    void handle_headers_synchronized(const code& ec, size_t block_height,
        result_handler handler);
    void handle_blocks_synchronized(const code& ec, size_t start_height,
        result_handler handler);

    hash_list hashes_;
    configuration configuration_;

    // TODO: move into blockchain constuctor.
    threadpool blockchain_pool_;
    blockchain::blockchain_impl blockchain_;
};

} // namspace node
} //namespace libbitcoin

#endif
