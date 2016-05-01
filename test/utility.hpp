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
  : public blockchain::block_chain
{
public:
    blockchain_fixture(bool import_result=true);
    virtual void start(result_handler handler);
    virtual void stop(result_handler handler);
    virtual void close();
    virtual bool import(chain::block::ptr block, uint64_t height);
    virtual void store(chain::block::ptr block, block_store_handler handler);
    virtual void fetch_block_header(uint64_t height,
        block_header_fetch_handler handler);
    virtual void fetch_block_header(const hash_digest& hash,
        block_header_fetch_handler handler);
    virtual void fetch_block_locator(block_locator_fetch_handler handle_fetch);
    virtual void fetch_locator_block_hashes(const message::get_blocks& locator,
        const hash_digest& threshold, size_t limit,
        locator_block_hashes_fetch_handler handler);
    virtual void fetch_missing_block_hashes(const hash_list& hashes,
        missing_block_hashes_fetch_handler handler);
    virtual void fetch_block_transaction_hashes(uint64_t height,
        transaction_hashes_fetch_handler handler);
    virtual void fetch_block_transaction_hashes(const hash_digest& hash,
        transaction_hashes_fetch_handler handler);
    virtual void fetch_block_height(const hash_digest& hash,
        block_height_fetch_handler handler);
    virtual void fetch_last_height(last_height_fetch_handler handler);
    virtual void fetch_transaction(const hash_digest& hash,
        transaction_fetch_handler handler);
    virtual void fetch_transaction_index(const hash_digest& hash,
        transaction_index_fetch_handler handler);
    virtual void fetch_spend(const chain::output_point& outpoint,
        spend_fetch_handler handler);
    virtual void fetch_history(const wallet::payment_address& address,
        uint64_t limit, uint64_t from_height, history_fetch_handler handler);
    virtual void fetch_stealth(const binary& prefix, uint64_t from_height,
        stealth_fetch_handler handler);
    virtual void subscribe_reorganize(
        blockchain::organizer::reorganize_handler handler);
private:
    bool import_result_;
};

} // namespace test
} // namespace node
} // namespace libbitcoin

#endif