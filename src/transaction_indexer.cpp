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
#include <bitcoin/node/transaction_indexer.hpp>

#include <bitcoin/blockchain.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::chain;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

transaction_indexer::transaction_indexer(threadpool& pool)
  : strand_(pool)
{
}

void transaction_indexer::query(const payment_address& address,
    query_handler handle_query)
{
    strand_.queue(
        std::bind(&transaction_indexer::do_query,
            this, address, handle_query));
}

template <typename InfoList, typename EntryMultimap>
InfoList get_info_list(const payment_address& address, EntryMultimap& map)
{
    InfoList info;
    auto iter_pair = map.equal_range(address);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it)
        info.push_back(it->second);

    return info;
}
void transaction_indexer::do_query(const payment_address& address,
    query_handler handle_query)
{
    handle_query(std::error_code(),
        get_info_list<output_info_list>(address, outputs_map_),
        get_info_list<spend_info_list>(address, spends_map_));
}

template <typename Point, typename EntryMultimap>
auto find_entry(const payment_address& key, const Point& value_point,
    EntryMultimap& map) -> decltype(map.begin())
{
    // The entry should only occur once in the multimap.
    const auto pair = map.equal_range(key);
    for (auto it = pair.first; it != pair.second; ++it)
        if (it->second.point == value_point)
            return it;

    return map.end();
}

template <typename Point, typename EntryMultimap>
bool index_does_not_exist(const payment_address& key,
    const Point& value_point, EntryMultimap& map)
{
    return find_entry(key, value_point, map) == map.end();
}

void transaction_indexer::index(const transaction_type& tx,
    completion_handler handle_index)
{
    strand_.queue(
        std::bind(&transaction_indexer::do_index,
            this, tx, handle_index));
}

void transaction_indexer::do_index(const transaction_type& tx,
    completion_handler handle_index)
{
    const auto tx_hash = hash_transaction(tx);

    uint32_t index = 0;
    for (const auto& input: tx.inputs)
    {
        payment_address address;
        if (extract(address, input.script))
        {
            input_point point{tx_hash, index};
            BITCOIN_ASSERT_MSG(
                index_does_not_exist(address, point, spends_map_),
                "Transaction input is indexed duplicate times!");
            spends_map_.emplace(address,
                spend_info_type{point, input.previous_output});
        }

        ++index;
    }

    index = 0;
    for (const auto& output: tx.outputs)
    {
        payment_address address;
        if (extract(address, output.script))
        {
            output_point point{tx_hash, index};
            BITCOIN_ASSERT_MSG(
                index_does_not_exist(address, point, outputs_map_),
                "Transaction output is indexed duplicate times!");
            outputs_map_.emplace(address,
                output_info_type{point, output.value});
        }

        ++index;
    }

    handle_index(std::error_code());
}

void transaction_indexer::deindex(const transaction_type& tx,
    completion_handler handle_deindex)
{
    strand_.queue(
        std::bind(&transaction_indexer::do_deindex,
            this, tx, handle_deindex));
}

void transaction_indexer::do_deindex(const transaction_type& tx,
    completion_handler handle_deindex)
{
    const auto tx_hash = hash_transaction(tx);

    uint32_t index = 0;
    for (const auto& input: tx.inputs)
    {
        payment_address address;
        if (extract(address, input.script))
        {
            input_point point{tx_hash, index};
            const auto entry = find_entry(address, point, spends_map_);
            BITCOIN_ASSERT_MSG(entry != spends_map_.end(),
                "Can't deindex transaction input twice");

            spends_map_.erase(entry);
            BITCOIN_ASSERT_MSG(
                index_does_not_exist(address, point, spends_map_), 
                "Transaction input is indexed duplicate times!");
        }

        ++index;
    }

    index = 0;
    for (const auto& output: tx.outputs)
    {
        payment_address address;
        if (extract(address, output.script))
        {
            output_point point{tx_hash, index};
            const auto entry = find_entry(address, point, outputs_map_);
            BITCOIN_ASSERT_MSG(entry != outputs_map_.end(),
                "Can't deindex transaction output twice");

            outputs_map_.erase(entry);
            BITCOIN_ASSERT_MSG(
                index_does_not_exist(address, point, outputs_map_), 
                "Transaction output is indexed duplicate times!");
        }

        ++index;
    }

    handle_deindex(std::error_code());
}

static bool is_output_conflict(history_list& history,
    const output_info_type& output)
{
    // Usually the indexer and memory doesn't have any transactions indexed tha
    // are already confirmed and in the blockchain. This is a rare corner case.
    for (const auto& row: history)
        if (row.id == point_ident::output && row.point == output.point)
            return true;

    return false;
}

static bool is_spend_conflict(history_list& history,
    const spend_info_type& spend)
{
    for (const auto& row: history)
        if (row.id == point_ident::spend && row.point == spend.point)
            return true;

    return false;
}

static void add_history_output(history_list& history,
    const output_info_type& output)
{
    history.emplace_back(history_row
    {
        point_ident::output, output.point, 0, { output.value }
    });
}

static void add_history_spend(history_list& history,
    const spend_info_type& spend)
{
    history.emplace_back(history_row
    {
        point_ident::spend, spend.point, 0, 
        { chain::spend_checksum(spend.previous_output) }
    });
}

static void add_history_outputs(history_list& history,
    const output_info_list& outputs)
{
    // If everything okay insert the outpoint.
    for (const auto& output: outputs)
        if (!is_output_conflict(history, output))
            add_history_output(history, output);
}

static void add_history_spends(history_list& history,
    const spend_info_list& spends)
{
    // If everything okay insert the spend.
    for (const auto& spend: spends)
        if (!is_spend_conflict(history, spend))
            add_history_spend(history, spend);

    // This assert can be triggered if the pool fills and starts dropping txs.
    // In practice this should not happen often and isn't a problem.
    //BITCOIN_ASSERT_MSG(!conflict, "Couldn't find output for adding spend");
}

void indexer_history_fetched(const std::error_code& ec,
    const output_info_list& outputs, const spend_info_list& spends,
    history_list history, blockchain::fetch_handler_history handle_fetch)
{
    // TODO: Why is "history_list history" passed in here, and not a reference?

    if (ec)
    {
        // Shouldn't "history" be returned here?
        handle_fetch(ec, history_list());
        return;
    }

    // There is always a chance of inconsistency, so we resolve these 
    // conflicts and move on. This can happen when new blocks arrive in,
    // and indexer.query() is called midway through a bunch of 
    // txpool.try_delete() operations. If do_query() is queued before
    // the last do_doindex() and there's a transaction in our query in 
    // that block then we will have a conflict.

    // Add all outputs and spends and return success code.
    add_history_outputs(history, outputs);
    add_history_spends(history, spends);
    handle_fetch(std::error_code(), history);
}

void blockchain_history_fetched(const std::error_code& ec,
    const history_list& history, transaction_indexer& indexer,
    const payment_address& address,
    blockchain::fetch_handler_history handle_fetch)
{
    if (ec)
    {
        handle_fetch(ec, chain::history_list());
        return;
    }

    indexer.query(address,
        std::bind(indexer_history_fetched,
            _1, _2, _3, history, handle_fetch));
}

// Fetch the history first from the blockchain and then from the indexer.
void fetch_history(blockchain& chain, transaction_indexer& indexer,
    const payment_address& address, 
    blockchain::fetch_handler_history handle_fetch, size_t from_height)
{
    chain.fetch_history(address,
        std::bind(blockchain_history_fetched,
            _1, _2, std::ref(indexer), address, handle_fetch), from_height);
}

} // namespace node
} // namespace libbitcoin

