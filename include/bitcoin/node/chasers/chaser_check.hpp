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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_CHECK_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_CHECK_HPP

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks for the candidate header chain.
class BCN_API chaser_check
  : public chaser
{
public:
    DELETE_COPY_MOVE(chaser_check);

    chaser_check(full_node& node) NOEXCEPT;
    virtual ~chaser_check() NOEXCEPT;

    virtual code start() NOEXCEPT;

    virtual void get_hashes(network::result_handler&& handler) NOEXCEPT;
    virtual void put_hashes(network::result_handler&& handler) NOEXCEPT;

protected:
    /// Handlers.
    virtual void handle_header(height_t branch_point) NOEXCEPT;
    virtual void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT;

    virtual void do_get_hashes(const network::result_handler& handler) NOEXCEPT;
    virtual void do_put_hashes(const network::result_handler& handler) NOEXCEPT;

private:
    void do_handle_event(const code& ec, chase event_, link value) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
