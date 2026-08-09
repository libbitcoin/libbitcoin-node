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
#include <bitcoin/node/validate.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;

code validate_transaction(const chain::transaction& tx, const query& query,
    const chain::context& pool) NOEXCEPT
{
    code ec{};

    // Ensure tx does not violate tx consensus rules.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(pool);
    if (!ec) query.populate_with_metadata(tx, true);
    if (!ec) ec = tx.accept(pool);
    if (!ec) ec = tx.confirm(pool);
    if (!ec) ec = tx.connect(pool);

    // Ensure tx does not violate presumed block consensus rules.
    // This is a DoS guard when validating a tx outside of a block.
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(pool);
    if (!ec) ec = tx.accept_guard(pool);
    return ec;
}

} // namespace node
} // namespace libbitcoin
