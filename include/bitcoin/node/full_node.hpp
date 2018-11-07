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
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/utility/reservations.hpp>

namespace libbitcoin {
namespace node {

/// A full node on the Bitcoin P2P network.
class BCN_API full_node
  : public network::p2p
{
public:
    typedef std::shared_ptr<full_node> ptr;
    typedef blockchain::block_chain::header_handler header_handler;
    typedef blockchain::block_chain::block_handler block_handler;
    typedef blockchain::block_chain::transaction_handler transaction_handler;

    /// Construct the full node.
    full_node(const configuration& configuration);

    /// Ensure all threads are coalesced.
    virtual ~full_node();

    // Start/Run sequences.
    // ------------------------------------------------------------------------

    /// Invoke startup and seeding sequence, call from constructing thread.
    void start(result_handler handler) override;

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    void run(result_handler handler) override;

    // Shutdown.
    // ------------------------------------------------------------------------

    /// Idempotent call to signal work stop, start may be reinvoked after.
    /// Returns the result of file save operation.
    bool stop() override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    bool close() override;

    // Properties.
    // ------------------------------------------------------------------------

    /// Node configuration settings.
    virtual const node::settings& node_settings() const;

    /// Node configuration settings.
    virtual const blockchain::settings& chain_settings() const;

    /// Blockchain query interface.
    virtual blockchain::safe_chain& chain();

    /// Get a download reservation manager.
    virtual reservation::ptr get_reservation();

    // Subscriptions.
    // ------------------------------------------------------------------------

    /// Subscribe to block reorganization and stop events.
    virtual void subscribe_blocks(block_handler&& handler);

    /// Subscribe to header reorganization and stop events.
    virtual void subscribe_headers(header_handler&& handler);

    /// Subscribe to transaction pool acceptance and stop events.
    virtual void subscribe_transactions(transaction_handler&& handler);

protected:
    /// Attach a node::session to the network, caller must start the session.
    template <class Session, typename... Args>
    typename Session::ptr attach(Args&&... args)
    {
        return std::make_shared<Session>(*this, std::forward<Args>(args)...);
    }

    /// Override to attach specialized p2p sessions.
    ////network::session_seed::ptr attach_seed_session() override;
    network::session_manual::ptr attach_manual_session() override;
    network::session_inbound::ptr attach_inbound_session() override;
    network::session_outbound::ptr attach_outbound_session() override;

private:
    bool handle_reindexed(code ec, size_t fork_height,
        header_const_ptr_list_const_ptr incoming,
        header_const_ptr_list_const_ptr outgoing);

    bool handle_reorganized(code ec, size_t fork_height,
        block_const_ptr_list_const_ptr incoming,
        block_const_ptr_list_const_ptr outgoing);

    void handle_running(const code& ec, result_handler handler);

    // These are thread safe.
    reservations reservations_;
    blockchain::block_chain chain_;
    const uint32_t protocol_maximum_;
    const node::settings& node_settings_;
    const blockchain::settings& chain_settings_;
};

} // namespace node
} // namespace libbitcoin

#endif
