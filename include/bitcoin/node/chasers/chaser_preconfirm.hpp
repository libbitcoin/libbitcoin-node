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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_PRECONFIRM_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_PRECONFIRM_HPP

#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks in the the candidate header chain for validation.
class BCN_API chaser_preconfirm
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_preconfirm);

    chaser_preconfirm(full_node& node) NOEXCEPT;

    virtual code start() NOEXCEPT;

protected:
    virtual void handle_disorganized(height_t fork_point) NOEXCEPT;
    virtual void handle_checked(height_t height) NOEXCEPT;
    virtual void handle_event(const code& ec, chase event_,
        link value) NOEXCEPT;

private:
    using block_ptr = system::chain::block::cptr;

    void do_checked() NOEXCEPT;
    bool is_under_milestone(size_t height) const NOEXCEPT;
    code validate(const block_ptr& block,
        const database::context& ctx) const NOEXCEPT;

    // These are thread safe.
    const system::chain::checkpoint milestone_;
    const system::chain::checkpoints checkpoints_;
    const uint64_t initial_subsidy_;
    const uint32_t subsidy_interval_blocks_;

    // This is protected by strand.
    size_t last_{};
};

} // namespace node
} // namespace libbitcoin

#endif
