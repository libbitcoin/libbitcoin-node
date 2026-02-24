/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

#include <atomic>
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
    virtual void do_prune(header_t link) NOEXCEPT;
    virtual void do_snap(size_t height) NOEXCEPT;
    ////virtual void do_archive(height_t height) NOEXCEPT;
    ////virtual void do_valid(height_t height) NOEXCEPT;
    ////virtual void do_confirm(height_t height) NOEXCEPT;

    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

private:
    void take_snapshot(height_t height) NOEXCEPT;
    ////bool update_bytes() NOEXCEPT;
    ////bool update_valid(height_t height) NOEXCEPT;
    ////bool update_confirm(height_t height) NOEXCEPT;

    ////// These are thread safe.
    ////const uint64_t snapshot_bytes_;
    ////const size_t snapshot_valid_;
    ////const size_t snapshot_confirm_;
    ////const bool enabled_bytes_;
    ////const bool enabled_valid_;
    ////const bool enabled_confirm_;

    // These are protected by strand.
    ////uint64_t bytes_{};
    ////size_t valid_{};
    ////size_t confirm_{};
    ////bool recent_{};
    std::atomic_bool pruned_{};
};

} // namespace node
} // namespace libbitcoin

#endif
