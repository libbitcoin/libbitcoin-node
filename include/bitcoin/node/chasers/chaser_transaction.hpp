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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TRANSACTION_HPP

#include <bitcoin/system.hpp>
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

    virtual void store(const system::chain::transaction::cptr& block) NOEXCEPT;

protected:
    virtual void handle_event(const code& ec, chase event_,
        event_link value) NOEXCEPT;

    virtual void do_confirmed(header_t link) NOEXCEPT;
    virtual void do_store(
        const system::chain::transaction::cptr& header) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
