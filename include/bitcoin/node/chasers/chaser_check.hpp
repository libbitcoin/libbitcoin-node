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
#include <memory>
#include <queue>
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
    typedef std::shared_ptr<database::associations> map_ptr;
    typedef std::function<void(const code&, const map_ptr&)> handler;
    typedef std::list<map_ptr> maps;

    DELETE_COPY_MOVE_DESTRUCT(chaser_check);

    chaser_check(full_node& node) NOEXCEPT;

    virtual code start() NOEXCEPT;

    virtual void get_hashes(handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

protected:
    virtual void handle_put_hashes(const code&) NOEXCEPT;
    virtual void handle_header(height_t branch_point) NOEXCEPT;
    virtual void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT;

    virtual void do_get_hashes(const handler& handler) NOEXCEPT;
    virtual void do_put_hashes(const map_ptr& map,
        const network::result_handler& handler) NOEXCEPT;

private:
    size_t update_table(maps& table, size_t start) const NOEXCEPT;
    size_t count_map(const maps& table) const NOEXCEPT;
    map_ptr make_map(size_t start, size_t count) const NOEXCEPT;
    map_ptr get_map(maps& table) NOEXCEPT;

    // These are thread safe.
    const size_t connections_;
    const size_t inventory_;

    // This is protected by strand.
    maps map_table_{};
};

} // namespace node
} // namespace libbitcoin

#endif
