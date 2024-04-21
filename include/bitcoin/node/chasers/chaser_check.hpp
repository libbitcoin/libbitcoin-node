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

#include <deque>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Maintain set of pending downloads identifiers for candidate header chain.
class BCN_API chaser_check
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_check);

    /// Craete empty shared map.
    static map_ptr empty_map() NOEXCEPT;

    /// Move half of map into returned map.
    static map_ptr split(const map_ptr& map) NOEXCEPT;

    chaser_check(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    code start() NOEXCEPT override;

    /// Interface for protocols to obtain/return pending download identifiers.
    /// Identifiers not downloaded must be returned or chain will remain gapped.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

protected:
    virtual void handle_event(const code& ec, chase event_,
        event_link value) NOEXCEPT;

    virtual void do_malleated(header_t link) NOEXCEPT;
    virtual void do_purge_headers(height_t top) NOEXCEPT;
    virtual void do_add_headers(height_t branch_point) NOEXCEPT;
    virtual void do_get_hashes(const map_handler& handler) NOEXCEPT;
    virtual void do_put_hashes(const map_ptr& map,
        const network::result_handler& handler) NOEXCEPT;

private:
    typedef std::deque<map_ptr> maps;

    ////size_t count_maps(const maps& table) const NOEXCEPT;
    size_t get_unassociated(maps& table, size_t start) const NOEXCEPT;
    map_ptr get_map(maps& table) NOEXCEPT;

    // These are thread safe.
    const size_t connections_;
    const size_t inventory_;

    // This is protected by strand.
    maps maps_{};
};

} // namespace node
} // namespace libbitcoin

#endif
