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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_VALIDATE_HPP

#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks in the the candidate header chain for validation.
class BCN_API chaser_validate
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_validate);

    chaser_validate(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

protected:
    typedef network::race_unity<const code&, const database::tx_link&> race;

    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_regressed(height_t branch_point) NOEXCEPT;
    virtual void do_disorganized(height_t top) NOEXCEPT;
    virtual void do_checked(height_t height) NOEXCEPT;
    virtual void do_bump(height_t height) NOEXCEPT;

////#if defined(UNDEFINED)
    virtual bool enqueue_block(const database::header_link& link) NOEXCEPT;
    virtual void validate_tx(const database::context& context,
        const database::tx_link& link, const race::ptr& racer) NOEXCEPT;
    virtual void handle_tx(const code& ec, const database::tx_link& tx,
        const race::ptr& racer) NOEXCEPT;
    virtual void handle_txs(const code& ec, const database::tx_link& tx,
        const database::header_link& link,
        const database::context& ctx) NOEXCEPT;
    virtual void validate_block(const code& ec,
        const database::header_link& link,
        const database::context& ctx) NOEXCEPT;
////#endif // UNDEFINED

private:
    code validate(const database::header_link& link, size_t height) NOEXCEPT;

    // neutrino
    system::hash_digest get_neutrino(size_t height) const NOEXCEPT;
    bool update_neutrino(const database::header_link& link) NOEXCEPT;
    bool update_neutrino(const database::header_link& link,
        const system::chain::block& block) NOEXCEPT;
    void update_position(size_t height) NOEXCEPT;

    // These are thread safe.
    const uint64_t initial_subsidy_;
    const uint32_t subsidy_interval_blocks_;

    // These are protected by strand.
    network::threadpool threadpool_;
    system::hash_digest neutrino_{};
};

} // namespace node
} // namespace libbitcoin

#endif
