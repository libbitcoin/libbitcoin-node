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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_POPULATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_POPULATE_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Order and populate downloaded non-bypass blocks for validation.
class BCN_API chaser_populate
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_populate);

    chaser_populate(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    code start() NOEXCEPT override;

    /// Populate a candidate block for validation.
    virtual void populate(const system::chain::block::cptr& block,
        const database::header_link& link, size_t height,
        network::result_handler&& complete) NOEXCEPT;

protected:
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_populate(const system::chain::block::cptr& block,
        database::header_link::integer link, size_t height,
        const network::result_handler& complete) NOEXCEPT;

private:
    // These are protected by strand.
    network::threadpool threadpool_;
    network::asio::strand independent_strand_;
};

} // namespace node
} // namespace libbitcoin

#endif
