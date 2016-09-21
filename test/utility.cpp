/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "utility.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {
namespace test {

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::config;

const config::checkpoint check0
{
    null_hash, 0
};

const config::checkpoint check42
{
    "4242424242424242424242424242424242424242424242424242424242424242", 42
};

const config::checkpoint::list no_checks;
const config::checkpoint::list one_check{ check42 };

// Create a headers message of specified size, starting with a genesis header.
message::headers::ptr message_factory(size_t count)
{
    return message_factory(count, null_hash);
}

// Create a headers message of specified size, using specified previous hash.
message::headers::ptr message_factory(size_t count, 
    const hash_digest& previous)
{
    auto previous_hash = previous;
    const auto headers = std::make_shared<message::headers>();
    auto& elements = headers->elements;

    for (size_t height = 0; height < count; ++height)
    {
        const header current_header{ 0, previous_hash, {}, 0, 0, 0, 0 };
        elements.push_back(current_header);
        previous_hash = current_header.hash();
    }

    return headers;
}

reservation_fixture::reservation_fixture(reservations& reservations,
    size_t slot, uint32_t block_timeout_seconds,
    clock::time_point now)
  : reservation(reservations, slot, block_timeout_seconds),
    now_(now)
{
}

// Accessor
std::chrono::microseconds reservation_fixture::rate_window() const
{
    return reservation::rate_window();
}

// Accessor
bool reservation_fixture::pending() const
{
    return reservation::pending();
}

// Accessor
void reservation_fixture::set_pending(bool value)
{
    reservation::set_pending(value);
}

// Stub
std::chrono::high_resolution_clock::time_point reservation_fixture::now() const
{
    return now_;
}

// ----------------------------------------------------------------------------

blockchain_fixture::blockchain_fixture(bool import_result, size_t gap_trigger,
    size_t gap_height)
  : import_result_(import_result),
    gap_trigger_(gap_trigger),
    gap_height_(gap_height)
{
}

bool blockchain_fixture::get_gap_range(uint64_t& out_first,
    uint64_t& out_last) const
{
    return false;
}

bool blockchain_fixture::get_next_gap(uint64_t& out_height,
    uint64_t start_height) const
{
    if (start_height == gap_trigger_)
    {
        out_height = gap_height_;
        return true;
    }

    return false;
}

bool blockchain_fixture::get_difficulty(hash_number& out_difficulty,
    uint64_t height) const
{
    return false;
}

bool blockchain_fixture::get_header(header& out_header, uint64_t height) const
{
    return false;
}

bool blockchain_fixture::get_height(uint64_t& out_height,
    const hash_digest& block_hash) const
{
    return false;
}

bool blockchain_fixture::get_bits(uint32_t& out_bits,
    const uint64_t& height) const
{
    return false;
}

bool blockchain_fixture::get_timestamp(uint32_t& out_timestamp,
    const uint64_t& height) const
{
    return false;
}

bool blockchain_fixture::get_version(uint32_t& out_version,
    const uint64_t& height) const
{
    return false;
}

bool blockchain_fixture::get_last_height(uint64_t& out_height) const
{
    return false;
}

bool blockchain_fixture::get_transaction_hash(hash_digest& out_transaction,
    const output_point& outpoint) const
{
    return false;
}

bool blockchain_fixture::get_transaction_height(uint64_t& out_block_height,
    const hash_digest& transaction_hash) const
{
    return false;
}

transaction_ptr blockchain_fixture::get_transaction(uint64_t& out_block_height,
    const hash_digest& transaction_hash) const
{
    return nullptr;
}

bool blockchain_fixture::insert(block_const_ptr block, uint64_t height)
{
    // This prevents a zero import cost, which is useful in testing timeout.
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return import_result_;
}
bool blockchain_fixture::push(block_const_ptr block, size_t height)
{
    return false;
}

bool blockchain_fixture::pop_from(block_const_ptr_list& out_blocks,
    uint64_t height)
{
    return false;
}

} // namespace test
} // namespace node
} // namespace libbitcoin
