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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Abstract base chaser.

/// Chasers impose order on blockchain/pool construction as necessary.

/// Each chaser operates on its own strand, implemented here, allowing
/// concurrent chaser operations to the extent that threads are available.

/// Events are passed between chasers using the full_node shared notifier.
/// Notifications are bounced from sink (e.g. chaser) to its strand, and
/// full_node::notify bounces from source (e.g. chaser) to network strand.

class BCN_API chaser
  : public network::enable_shared_from_base<chaser>,
    public network::reporter
{
public:
    DELETE_COPY_MOVE(chaser);

    /// True if the current thread is on the chaser strand.
    virtual bool stranded() const NOEXCEPT;

protected:
    chaser(full_node& node) NOEXCEPT;
    virtual ~chaser() NOEXCEPT;

private:
    // These are thread safe (mostly).
    full_node& node_;
    network::asio::strand strand_;
};

} // namespace node
} // namespace libbitcoin

#endif
