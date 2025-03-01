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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_CONFIRM_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_CONFIRM_HPP

#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

////#define SEQUENTIAL

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down valid blocks for confirmation.
class BCN_API chaser_confirm
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_confirm);

    chaser_confirm(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;
    void stop() NOEXCEPT override;

protected:
    using header_links = std_vector<database::header_link>;
    typedef network::race_unity<const code&, const database::tx_link&> race;

    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    ////virtual void do_checking(height_t height) NOEXCEPT;
    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_validated(height_t height) NOEXCEPT;
    virtual void do_bump(height_t branch_point) NOEXCEPT;

    ////virtual void do_reorganize(header_links& fork, size_t fork_point) NOEXCEPT;
    ////virtual void do_organize(header_links& fork, const header_links& popped,
    ////    size_t fork_point) NOEXCEPT;

    // Override base class strand because it sits on the network thread pool.
    network::asio::strand& strand() NOEXCEPT override;
    bool stranded() const NOEXCEPT override;

private:
    struct neutrino_header
    {
        system::hash_digest head{};
        database::header_link link{};
    };

    bool set_organized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool reset_organized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool set_reorganized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool roll_back(const header_links& popped, size_t fork_point,
        size_t top) NOEXCEPT;

    bool get_fork_work(uint256_t& fork_work, header_links& fork,
        height_t fork_top) const NOEXCEPT;
    bool get_is_strong(bool& strong, const uint256_t& fork_work,
        size_t fork_point) const NOEXCEPT;

    // These are protected by strand.
    network::threadpool threadpool_;

    // These are thread safe.
    network::asio::strand independent_strand_;
};

} // namespace node
} // namespace libbitcoin

#endif
