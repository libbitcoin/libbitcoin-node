/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_MESSAGES_TRANSACTION_HPP
#define LIBBITCOIN_NODE_MESSAGES_TRANSACTION_HPP

#include <optional>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
namespace messages {

/// Based on network::messages::peer::transaction.
struct BCN_API transaction
{
    typedef std::shared_ptr<const transaction> cptr;

    static const network::messages::peer::identifier id;
    static const std::string command;
    static const uint32_t version_minimum;
    static const uint32_t version_maximum;

    // TODO: optimized translation direct to store.
    ////static cptr deserialize(uint32_t version, const system::data_chunk& data,
    ////    bool witness=true) NOEXCEPT;
    ////static transaction deserialize(uint32_t version, system::reader& source,
    ////    bool witness=true) NOEXCEPT;

    /// These return false if witness or version is inconsistent with tx data.
    bool serialize(uint32_t version, const system::data_slab& data,
        bool witness=true) const NOEXCEPT;
    void serialize(uint32_t version, system::writer& sink,
        bool witness=true) const NOEXCEPT;
    size_t size(uint32_t version, bool witness=true) const NOEXCEPT;

    /// Wire serialized transaction.
    system::data_chunk tx_data{};

    /// Non-witness hash of the transaction (for non-witness send optimize).
    system::hash_digest hash{};

    /// Transaction contains witness data (if applicable).
    const bool witnessed_{};
};

} // namespace messages
} // namespace node
} // namespace libbitcoin

#endif
