/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <unordered_map>
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

    /// Create empty shared map.
    static map_ptr empty_map() NOEXCEPT;

    /// Move half of map into returned map.
    static map_ptr split(const map_ptr& map) NOEXCEPT;

    chaser_check(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

    /// Interface for protocols to provide performance data.
    virtual void update(object_key channel, uint64_t speed,
        network::result_handler&& handler) NOEXCEPT;

    /// Interface for protocols to obtain/return pending download identifiers.
    /// Identifiers not downloaded must be returned or chain will remain gapped.
    virtual void get_hashes(map_handler&& handler) NOEXCEPT;
    virtual void put_hashes(const map_ptr& map,
        network::result_handler&& handler) NOEXCEPT;

protected:
    virtual void handle_purged(const code& ec) NOEXCEPT;
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    /// block tracking
    virtual void do_bump(height_t height) NOEXCEPT;
    virtual void do_checked(height_t height) NOEXCEPT;
    virtual void do_advanced(height_t height) NOEXCEPT;
    virtual void do_headers(height_t branch_point) NOEXCEPT;
    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_handle_purged(const code& ec) NOEXCEPT;
    virtual void do_get_hashes(const map_handler& handler) NOEXCEPT;
    virtual void do_put_hashes(const map_ptr& map,
        const network::result_handler& handler) NOEXCEPT;

    /// channel performance
    virtual void do_starved(object_t self) NOEXCEPT;
    virtual void do_update(object_key channel, uint64_t speed,
        const network::result_handler& handler) NOEXCEPT;

private:
    static constexpr size_t minimum_for_standard_deviation = 3;
    typedef std::unordered_map<object_key, double> speeds;
    typedef std::deque<map_ptr> maps;

    map_ptr get_map() NOEXCEPT;
    size_t set_unassociated() NOEXCEPT;
    size_t get_inventory_size() const NOEXCEPT;
    bool set_map(const map_ptr& map) NOEXCEPT;

    void start_tracking() NOEXCEPT;
    void stop_tracking() NOEXCEPT;
    bool purging() const NOEXCEPT;

    // These are thread safe.
    const size_t maximum_concurrency_;
    const size_t maximum_height_;
    const size_t connections_;
    const float allowed_deviation_;

    // These are protected by strand.
    size_t inventory_{};
    size_t requested_{};
    size_t advanced_{};
    job::ptr job_{};

    // TODO: optimize, default bucket count is around 8.
    speeds speeds_{};
    maps maps_{};
};

} // namespace node
} // namespace libbitcoin

#endif
