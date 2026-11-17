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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_TEMPLATE_HPP

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Construct template blocks upon request.
class BCN_API chaser_template
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_template);

    chaser_template(full_node& node) NOEXCEPT;

    code start() NOEXCEPT override;

    /// A template of pooled txs built upon the request, parents first, and
    /// its height.
    virtual void get_template(template_handler&& handler) NOEXCEPT;

protected:
    virtual void do_get_template(const template_handler& handler) NOEXCEPT;

private:
    void handle_build(const code& ec, size_t height,
        const database::tx_links& links,
        const template_handler& handler) NOEXCEPT;
    void do_build(const code& ec, size_t height,
        const database::tx_links& links,
        const template_handler& handler) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
