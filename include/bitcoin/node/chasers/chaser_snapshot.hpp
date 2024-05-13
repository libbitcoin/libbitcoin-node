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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_SNAPSHOT_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_SNAPSHOT_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Perform automated snapshots.
class BCN_API chaser_snapshot
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_snapshot);

    chaser_snapshot(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

protected:
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_archive(height_t height) NOEXCEPT;
    virtual void do_confirm(height_t height) NOEXCEPT;
    virtual void do_snap(height_t height) NOEXCEPT;

private:
    void do_snapshot(height_t height) NOEXCEPT;

    // These are thread safe.
    const size_t snapshot_interval_;
    size_t current_archive_{};
    size_t current_confirm_{};
};

} // namespace node
} // namespace libbitcoin

#endif
