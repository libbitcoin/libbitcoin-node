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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_STORAGE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_STORAGE_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Monitor storage capacity following a disk full condition.
/// Clear disk full condition and restart network given increased capacity.
class BCN_API chaser_storage
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_storage);

    chaser_storage(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

protected:
    virtual void do_full(size_t height) NOEXCEPT;
    virtual void do_stop(size_t height) NOEXCEPT;
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

private:
    void handle_timer(const code& ec) NOEXCEPT;
    bool is_full() const NOEXCEPT;

    network::deadline::ptr disk_timer_;
};

} // namespace node
} // namespace libbitcoin

#endif
