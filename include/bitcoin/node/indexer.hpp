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
#ifndef LIBBITCOIN_NODE_INDEXER_HPP
#define LIBBITCOIN_NODE_INDEXER_HPP

#include <cstddef>
#include <forward_list>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/define.hpp>

// Allow payment_address to be in indexed in std::*map classes.
namespace std {
    template <>
    struct hash<bc::wallet::payment_address>
    {
        size_t operator()(const bc::wallet::payment_address& address) const
        {
            // Create a string of the form [address-version|address-hash].
            std::string buffer;
            buffer.resize(bc::short_hash_size + 1);
            buffer[0] = address.version();
            const auto& hash = address.hash();
            std::copy(hash.begin(), hash.end(), buffer.begin() + 1);
            std::hash<std::string> functor;
            return functor(buffer);
        }
    };

} // namspace std

namespace libbitcoin {
namespace node {

using namespace message;

// output_info_type is defined in <bitcoin/transaction.hpp>
struct BCN_API spend_info_type
{
    chain::input_point point;
    chain::output_point previous_output;
};

typedef std::vector<spend_info_type> spend_info_list;

class BCN_API indexer
{
public:

    typedef std::function<void (const code&)> completion_handler;
    typedef std::function<void (const code& ec,
        const wallet::output_info_list& outputs,
        const spend_info_list& spends)> query_handler;

    indexer(threadpool& pool);

    /// This class is not copyable.
    indexer(const indexer&) = delete;
    void operator=(const indexer&) = delete;

    /**
     * Query all transactions indexed that are related to a Bitcoin address.
     * @param[in]   address  Bitcoin address to lookup.
     * @param[in]   handler  Completion handler for fetch operation.
     */
    void query(const wallet::payment_address& address,
        query_handler handle_query);

    /**
     * Index a transaction.
     * @param[in]   tx       Transaction to index.
     * @param[in]   handler  Completion handler for index operation.
     */
    void index(const chain::transaction& tx, completion_handler handler);

    /**
     * Deindex (remove from index) a transaction.
     * @param[in]   tx       Transaction to deindex.
     * @param[in]   handler  Completion handler for deindex operation.
     */
    void deindex(const chain::transaction& tx, completion_handler handler);

private:

    // addr -> spend
    typedef std::unordered_multimap<wallet::payment_address, spend_info_type>
        spends_multimap;

    // addr -> output
    typedef std::unordered_multimap<wallet::payment_address,
        wallet::output_info> outputs_multimap;

    void do_index(const chain::transaction& tx, completion_handler handler);
    void do_deindex(const chain::transaction& tx, completion_handler handler);
    void do_query(const wallet::payment_address& payaddr,
        query_handler handler);

    dispatcher dispatch_;
    spends_multimap spends_map_;
    outputs_multimap outputs_map_;
};

BCN_API void fetch_history(blockchain::block_chain& chain,
    indexer& indexer, const bc::wallet::payment_address& address,
    blockchain::block_chain::history_fetch_handler handler,
    size_t from_height=0);

} // namespace node
} // namespace libbitcoin

#endif
