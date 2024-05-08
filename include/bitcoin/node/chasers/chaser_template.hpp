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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Construct template blocks upon modification of the transaction DAG.
class BCN_API chaser_template
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_template);

    chaser_template(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

protected:
    virtual bool handle_event(const code& ec, chase event_,
        event_value value) NOEXCEPT;

    virtual void do_transaction(transaction_t value) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
