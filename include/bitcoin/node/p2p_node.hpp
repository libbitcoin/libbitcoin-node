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
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/hash_queue.hpp>

namespace libbitcoin {
namespace node {

#define LOG_NODE "node"

/// A full node on the Bitcoin P2P network.
class BCN_API p2p_node
  : public network::p2p
{
public:
    typedef std::shared_ptr<p2p_node> ptr;
    typedef blockchain::organizer::reorganize_handler reorganize_handler;
    typedef blockchain::transaction_pool::transaction_handler
        transaction_handler;

    /// Construct the full node.
    p2p_node(const configuration& configuration);

    /// Destruct the full node.
    ~p2p_node();

    // ------------------------------------------------------------------------

    /// Return a reference to the node configuration.
    virtual const settings& node_settings() const;

    /// Invoke startup and seeding sequence, call from constructing thread.
    virtual void start(result_handler handler) override;

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    virtual void run(result_handler handler) override;

    /// Subscribe to blockchain reorganization and stop events.
    virtual void subscribe_blockchain(reorganize_handler handler);

    /// Subscribe to transaction pool acceptance and stop events.
    virtual void subscribe_transaction_pool(transaction_handler handler);

    /// Non-blocking call to coalesce all work, start may be reinvoked after.
    /// Handler returns the result of file save operations.
    virtual void stop(result_handler handler) override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    virtual void close() override;

//// Must expose directly for now.
////protected:

    /// Blockchain query interface.
    virtual blockchain::block_chain& chain();

    /// Transaction pool interface.
    virtual blockchain::transaction_pool& pool();

private:
    void handle_stopped(const code& ec);
    void handle_blockchain_stopped(const code& ec, result_handler handler);
    void handle_blockchain_start(const code& ec, result_handler handler);
    void handle_fetch_header(const code& ec, const chain::header& block_header,
        size_t block_height, result_handler handler);
    void handle_headers_synchronized(const code& ec, result_handler handler);
    void handle_blocks_synchronized(const code& ec, result_handler handler);

    // This is thread safe and guarded by the non-restartable node constraint.
    hash_queue hashes_;

    // This is thread safe.
    blockchain::block_chain_impl blockchain_;

    const settings& settings_;
};

} // namspace node
} //namespace libbitcoin

#endif
