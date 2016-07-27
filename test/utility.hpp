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
#ifndef LIBBITCOIN_NODE_TEST_RESERVATIONS_HPP
#define LIBBITCOIN_NODE_TEST_RESERVATIONS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {
namespace test {

#define DECLARE_RESERVATIONS(name, import) \
config::checkpoint::list checkpoints; \
header_queue hashes(checkpoints); \
blockchain_fixture blockchain(import); \
node::settings settings; \
reservations name(hashes, blockchain, settings)

extern const config::checkpoint check42;
extern const config::checkpoint::list no_checks;
extern const config::checkpoint::list one_check;

// Create a headers message of specified size, using specified previous hash.
extern message::headers::ptr message_factory(size_t count);
extern message::headers::ptr message_factory(size_t count, const hash_digest& hash);

class reservation_fixture
  : public reservation
{
public:
    reservation_fixture(reservations& reservations, size_t slot,
        uint32_t block_timeout_seconds,
        std::chrono::high_resolution_clock::time_point
            now=std::chrono::high_resolution_clock::now());
    std::chrono::microseconds rate_window() const;
    std::chrono::high_resolution_clock::time_point now() const override;
    bool pending() const;
    void set_pending(bool value);

private:
    std::chrono::high_resolution_clock::time_point now_;
};

class blockchain_fixture
  : public blockchain::simple_chain
{
public:
    blockchain_fixture(bool import_result=true);
    bool get_next_gap(uint64_t& out_height, uint64_t start_height);
    bool get_difficulty(hash_number& out_difficulty, uint64_t height);
    bool get_header(chain::header& out_header, uint64_t height);
    bool get_height(uint64_t& out_height, const hash_digest& block_hash);
    bool get_last_height(size_t& out_height);
    bool get_outpoint_transaction(hash_digest& out_transaction,
        const chain::output_point& outpoint);
    bool get_transaction(chain::transaction& out_transaction,
        uint64_t& out_block_height, const hash_digest& transaction_hash);
    bool import(chain::block::ptr block, uint64_t height);
    bool push(blockchain::block_detail::ptr block);
    bool pop_from(blockchain::block_detail::list& out_blocks,
        uint64_t height);

private:
    bool import_result_;
};

} // namespace test
} // namespace node
} // namespace libbitcoin

#endif