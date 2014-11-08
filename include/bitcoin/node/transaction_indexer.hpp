/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_TRANSACTION_INDEXER_HPP
#define LIBBITCOIN_NODE_TRANSACTION_INDEXER_HPP

#include <forward_list>
#include <system_error>
#include <unordered_map>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/node/blockchain.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

// output_info_type is defined in <bitcoin/transaction.hpp>

struct BCN_API spend_info_type
{
    input_point point;
    output_point previous_output;
};

typedef std::vector<spend_info_type> spend_info_list;

class transaction_indexer
{
public:
    typedef std::function<void (const std::error_code&)> completion_handler;

    typedef std::function<void (const std::error_code& ec,
        const output_info_list& outputs, const spend_info_list& spends)>
            query_handler;

    BCN_API transaction_indexer(threadpool& pool);

    /// Non-copyable class
    transaction_indexer(const transaction_indexer&) = delete;
    /// Non-copyable class
    void operator=(const transaction_indexer&) = delete;

    /**
     * Query all transactions indexed that are related to a Bitcoin address.
     *
     * @param[in]   payaddr         Bitcoin address to lookup.
     * @param[in]   handle_query    Completion handler for fetch operation.
     * @code
     *  void handle_query(
     *      const std::error_code& ec,       // Status of operation
     *      const output_info_list& outputs, // Outputs
     *      const spend_info_list& spends    // Inputs
     *  );
     * @endcode
     * @code
     *  struct output_info_type
     *  {
     *      output_point point;
     *      uint64_t value;
     *  };
     *
     *  struct spend_info_type
     *  {
     *      input_point point;
     *      output_point previous_output;
     *  };
     * @endcode
     */
    BCN_API void query(const payment_address& payaddr,
        query_handler handle_query);

    /**
     * Index a transaction.
     *
     * @param[in]   tx              Transaction to index.
     * @param[in]   handle_index    Completion handler for index operation.
     * @code
     *  void handle_index(
     *      const std::error_code& ec        // Status of operation
     *  );
     * @endcode
     */
    BCN_API void index(const transaction_type& tx,
        completion_handler handle_index);

    /**
     * Deindex (remove from index) a transaction.
     *
     * @param[in]   tx              Transaction to deindex.
     * @param[in]   handle_index    Completion handler for deindex operation.
     * @code
     *  void handle_deindex(
     *      const std::error_code& ec        // Status of operation
     *  );
     * @endcode
     */
    BCN_API void deindex(const transaction_type& tx,
        completion_handler handle_deindex);

private:
    // addr -> spend
    typedef std::unordered_multimap<payment_address, spend_info_type>
        spends_multimap;
    // addr -> output
    typedef std::unordered_multimap<payment_address, output_info_type>
        outputs_multimap;

    void do_query(const payment_address& payaddr,
        query_handler handle_query);

    void do_index(const transaction_type& tx,
        completion_handler handle_index);

    void do_deindex(const transaction_type& tx,
        completion_handler handle_deindex);

    async_strand strand_;
    spends_multimap spends_map_;
    outputs_multimap outputs_map_;
};

BCN_API void fetch_history(chain::blockchain& chain, transaction_indexer& indexer,
    const payment_address& address,
    chain::blockchain::fetch_handler_history handle_fetch, size_t from_height=0);

} // namespace node
} // namespace libbitcoin

#endif

