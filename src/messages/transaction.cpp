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
#include <bitcoin/node/messages/transaction.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {
namespace messages {

using namespace system;
using namespace network::messages::peer;
    
const std::string transaction::command = "tx";
const identifier transaction::id = identifier::transaction;
const uint32_t transaction::version_minimum = level::minimum_protocol;
const uint32_t transaction::version_maximum = level::maximum_protocol;

// data_slab is preallocated after the message header using size().
bool transaction::serialize(uint32_t version, const data_slab& data,
    bool witness) const NOEXCEPT
{
    if (witness != witnessed_)
        return false;

    system::stream::out::fast out{ data };
    system::write::bytes::fast writer{ out };
    serialize(version, writer, witness);
    return writer;
}

// Sender must ensure that version/witness are consistent with channel.
void transaction::serialize(uint32_t, writer& sink,
    bool BC_DEBUG_ONLY(witness)) const NOEXCEPT
{
    BC_ASSERT(witness == witnessed_);
    sink.write_bytes(tx_data);
}

// Sender must ensure that version/witness are consistent with channel.
size_t transaction::size(uint32_t, bool BC_DEBUG_ONLY(witness)) const NOEXCEPT
{
    BC_ASSERT(witness == witnessed_);
    return tx_data.size();
}

} // namespace messages
} // namespace node
} // namespace libbitcoin
