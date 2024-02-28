/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

#include <memory>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API full_node
  : public network::p2p
{
public:
    typedef std::shared_ptr<full_node> ptr;
    typedef database::store<database::map> store;
    typedef database::query<store> query;

    /// Constructors.
    /// -----------------------------------------------------------------------

    /// Construct the node.
    full_node(query& query, const configuration& configuration,
        const network::logger& log) NOEXCEPT;

    ~full_node() NOEXCEPT;

    /// Sequences.
    /// -----------------------------------------------------------------------

    /// Start the node (seed and manual services).
    void start(network::result_handler&& handler) NOEXCEPT override;

    /// Run the node (inbound/outbound services and blockchain chasers).
    void run(network::result_handler&& handler) NOEXCEPT override;

    /// Close the node.
    void close() NOEXCEPT override;

    /// Chasers.
    /// -----------------------------------------------------------------------

    /// Organize a validated header.
    virtual void organize(const system::chain::header::cptr& header,
        chaser::organize_handler&& handler) NOEXCEPT;

    /// Organize a validated block.
    virtual void organize(const system::chain::block::cptr& block,
        chaser::organize_handler&& handler) NOEXCEPT;

    /// Manage download queue.
    virtual void get_hashes(chaser_check::handler&& handler) NOEXCEPT;
    virtual void put_hashes(const chaser_check::map& map,
        network::result_handler&& handler) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// Node configuration settings.
    const settings& node_settings() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

    /// Obtain reference to the chaser event subscriber.
    chaser::event_subscriber& event_subscriber() NOEXCEPT;

protected:
    /// Session attachments.
    /// -----------------------------------------------------------------------
    network::session_manual::ptr attach_manual_session() NOEXCEPT override;
    network::session_inbound::ptr attach_inbound_session() NOEXCEPT override;
    network::session_outbound::ptr attach_outbound_session() NOEXCEPT override;

    /// Virtual handlers.
    /// -----------------------------------------------------------------------
    void do_start(const network::result_handler& handler) NOEXCEPT override;
    void do_run(const network::result_handler& handler) NOEXCEPT override;
    void do_close() NOEXCEPT override;

private:
    // These are thread safe.
    const configuration& config_;
    query& query_;

    // These are protected by strand.
    chaser::event_subscriber event_subscriber_;
    chaser_block chaser_block_;
    chaser_header chaser_header_;
    chaser_check chaser_check_;
    chaser_connect chaser_connect_;
    chaser_confirm chaser_confirm_;
    chaser_transaction chaser_transaction_;
    chaser_candidate chaser_candidate_;
};

} // namespace node
} // namespace libbitcoin

#endif
