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
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class BCN_API full_node
  : public network::p2p
{
public:
    typedef database::store<database::map> store;
    typedef database::query<store> query;
    typedef std::shared_ptr<full_node> ptr;

    /// Construct the node.
    full_node(query& query, const configuration& configuration,
        const network::logger& log) NOEXCEPT;

    /// Start the node (seed and manual services).
    void start(network::result_handler&& handler) NOEXCEPT override;

    /// Run the node (inbound and outbound services).
    void run(network::result_handler&& handler) NOEXCEPT override;

    // Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    query& archive() const NOEXCEPT;

protected:
    network::session_manual::ptr attach_manual_session() NOEXCEPT override;
    network::session_inbound::ptr attach_inbound_session() NOEXCEPT override;
    network::session_outbound::ptr attach_outbound_session() NOEXCEPT override;

private:
    // These are thread safe.
    const configuration& config_;
    query& query_;
};

} // namespace node
} // namespace libbitcoin

#endif
