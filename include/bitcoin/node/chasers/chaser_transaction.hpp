/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down unconfirmed transactions.
class BCN_API chaser_transaction
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_transaction);

    chaser_transaction(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

    /// Validate and archive a submitted package, accepted as a whole.
    virtual void submit(const system::chain::transactions_cptr& txs,
        submit_handler&& handler) NOEXCEPT;

protected:
    virtual bool handle_chase(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_submit(const system::chain::transactions_cptr& txs,
        const submit_handler& handler) NOEXCEPT;

    /// Recompute the pool context, closing the pool if not current.
    virtual void do_bump() NOEXCEPT;

private:
    code validate(const system::chain::transaction& tx, const query& query,
        const system::chain::context& pool) NOEXCEPT;

    // These are protected by strand.
    system::chain::context pool_{};
    bool pooling_{};
};

} // namespace node
} // namespace libbitcoin

#endif
