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

protected:
    using header_links = std_vector<database::header_link>;
    typedef network::race_unity<const code&, const database::tx_link&> race;

    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_validated(height_t height) NOEXCEPT;
    virtual void do_reorganize(size_t height) NOEXCEPT;
    virtual void do_organize(size_t height) NOEXCEPT;
    virtual bool enqueue_block(const database::header_link& link) NOEXCEPT;
    virtual void confirm_tx(const database::context& ctx,
        const database::tx_link& link, const race::ptr& racer) NOEXCEPT;
    virtual void handle_tx(const code& ec, const database::tx_link& tx,
        const race::ptr& racer) NOEXCEPT;
    virtual void handle_txs(const code& ec, const database::tx_link& tx,
        const database::header_link& link, size_t height)NOEXCEPT;
    virtual void confirm_block(const code& ec,
        const database::header_link& link, size_t height) NOEXCEPT;
    virtual void next_block(size_t height) NOEXCEPT;

private:
    void reset() NOEXCEPT;
    bool busy() const NOEXCEPT;

    bool set_organized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool reset_organized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool set_reorganized(const database::header_link& link,
        height_t height) NOEXCEPT;
    bool roll_back(const header_links& popped, size_t fork_point,
        size_t top) NOEXCEPT;
    bool roll_back(const header_links& popped, size_t fork_point,
        size_t top, const database::header_link& link) NOEXCEPT;

    bool get_fork_work(uint256_t& fork_work, header_links& fork,
        height_t fork_top) const NOEXCEPT;
    bool get_is_strong(bool& strong, const uint256_t& fork_work,
        size_t fork_point) const NOEXCEPT;

    // These are protected by strand.
    network::threadpool threadpool_;
    header_links popped_{};
    header_links fork_{};
    size_t fork_point_{};
};

} // namespace node
} // namespace libbitcoin

#endif
