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

#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

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

    code start() NOEXCEPT override;

protected:
    virtual void handle_event(const code& ec, chase event_,
        event_link value) NOEXCEPT;

    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_disorganized(height_t top) NOEXCEPT;
    virtual void do_checked(height_t height) NOEXCEPT;
    virtual void do_bump(height_t height) NOEXCEPT;

private:
    code validate(const database::header_link& link,
        size_t height) const NOEXCEPT;

    // These are thread safe.
    const uint64_t initial_subsidy_;
    const uint32_t subsidy_interval_blocks_;

    // This is protected by strand.
    size_t validated_{};
};

} // namespace node
} // namespace libbitcoin

#endif
