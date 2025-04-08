/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include "executor.hpp"
#include "localize.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace network;
using namespace system;
using namespace std::chrono;
using namespace std::placeholders;

constexpr double to_double(auto integer)
{
    return 1.0 * integer;
}

// fork flag transitions (candidate chain).
void executor::scan_flags() const
{
    const auto start = logger::now();
    constexpr auto flag_bits = to_bits(sizeof(chain::flags));
    const auto error = code{ database::error::integrity }.message();
    const auto top = query_.get_top_candidate();
    uint32_t flags{};

    logger(BN_OPERATION_INTERRUPT);

    for (size_t height{}; !cancel_ && height <= top; ++height)
    {
        database::context ctx{};
        const auto link = query_.to_candidate(height);
        if (!query_.get_context(ctx, link) || (ctx.height != height))
        {
            logger(format("Error: %1%") % error);
            return;
        }

        if (ctx.flags != flags)
        {
            const binary prev{ flag_bits, to_big_endian(flags) };
            const binary next{ flag_bits, to_big_endian(ctx.flags) };
            logger(format("Forked from [%1%] to [%2%] at [%3%:%4%]") % prev %
                next % encode_hash(query_.get_header_key(link)) % height);
            flags = ctx.flags;
        }
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<milliseconds>(logger::now() - start);
    logger(format("Scanned %1% headers for rule forks in %2% ms.") % top %
        span.count());
}

// input and output table slab counts.
void executor::scan_slabs() const
{
    logger(BN_MEASURE_SLABS);
    logger(BN_OPERATION_INTERRUPT);
    database::tx_link::integer link{};
    size_t inputs{}, outputs{};
    const auto start = logger::now();
    constexpr auto frequency = 100'000;

    // Tx (record) links are sequential and so iterable, however the terminal
    // condition assumes all tx entries fully written (ok for stopped node).
    // A running node cannot safely iterate over record links, but stopped can.
    for (auto puts = query_.put_counts(link); to_bool(puts.first) && !cancel_;
        puts = query_.put_counts(++link))
    {
        inputs += puts.first;
        outputs += puts.second;
        if (is_zero(link % frequency))
            logger(format(BN_MEASURE_SLABS_ROW) % link % inputs % outputs);
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BN_MEASURE_STOP) % inputs % outputs % span.count());
}

// hashmap bucket fill rates.
void executor::scan_buckets() const
{
    constexpr auto block_frequency = 10'000u;
    constexpr auto tx_frequency = 1'000'000u;
    constexpr auto put_frequency = 10'000'000u;

    logger(BN_OPERATION_INTERRUPT);

    auto filled = zero;
    auto bucket = max_size_t;
    auto start = logger::now();
    while (!cancel_ && (++bucket < query_.header_buckets()))
    {
        const auto top = query_.top_header(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % block_frequency))
            logger(format("header" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    auto span = duration_cast<seconds>(logger::now() - start);
    logger(format("header" BN_READ_ROW) % (to_double(filled) / bucket) %
        span.count());

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!cancel_ && (++bucket < query_.txs_buckets()))
    {
        const auto top = query_.top_txs(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % block_frequency))
            logger(format("txs" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(format("txs" BN_READ_ROW) % (to_double(filled) / bucket) %
        span.count());

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!cancel_ && (++bucket < query_.tx_buckets()))
    {
        const auto top = query_.top_tx(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % tx_frequency))
            logger(format("tx" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(format("tx" BN_READ_ROW) % (to_double(filled) / bucket) %
        span.count());

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!cancel_ && (++bucket < query_.point_buckets()))
    {
        const auto top = query_.top_point(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % put_frequency))
            logger(format("point" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(format("point" BN_READ_ROW) % (to_double(filled) / bucket) %
        span.count());
}

// hashmap collision distributions.
// BUGBUG: the vector allocations are exceessive and can result in sigkill.
// BUGBUG: must process each header independently as buckets may not coincide.
void executor::scan_collisions() const
{
    using namespace database;
    using hint = header_link::integer;
    constexpr auto empty = 0u;
    constexpr auto block_frequency = 10'000u;
    constexpr auto tx_frequency = 1'000'000u;
    constexpr auto put_frequency = 10'000'000u;
    constexpr auto count = [](const auto& list)
    {
        return std::accumulate(list.begin(), list.end(), zero,
            [](size_t total, const auto& value)
            {
                return total + to_int(to_bool(value));
            });
    };
    constexpr auto dump = [&](const auto& list)
    {
        // map frequency to length.
        std::map<size_t, size_t> map{};
        for (const auto value: list)
            ++map[value];

        return map;
    };

    logger(BN_OPERATION_INTERRUPT);

    // header & txs (txs is a proxy for validated_bk)
    // ------------------------------------------------------------------------

    auto index = max_size_t;
    auto start = logger::now();
    const auto header_buckets = query_.header_buckets();
    const auto header_records = query_.header_records();
    std_vector<size_t> header(header_buckets, empty);
    std_vector<size_t> txs(header_buckets, empty);
    while (!cancel_ && (++index < header_records))
    {
        const header_link link{ possible_narrow_cast<hint>(index) };
        const auto key = query_.get_header_key(link.value);
        ++header.at(database::keys::hash(key) % header_buckets);
        ++txs.at(database::keys::hash(
            link.operator data_array<header_link::size>()) %
            header_buckets);

        if (is_zero(index % block_frequency))
            logger(format("header/txs" BN_READ_ROW) % index %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    // ........................................................................
    
    const auto header_count = count(header);
    auto span = duration_cast<seconds>(logger::now() - start);
    logger(format("header: %1% in %2%s buckets %3% filled %4% rate %5% ") %
        index % span.count() % header_buckets % header_count %
        (to_double(header_count) / header_buckets));

    for (const auto& entry: dump(header))
        logger(format("header: %1% frequency: %2%") %
            entry.first % entry.second);

    header.clear();
    header.shrink_to_fit();

    // ........................................................................

    const auto txs_count = count(txs);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("txs: %1% in %2%s buckets %3% filled %4% rate %5%") %
        index % span.count() % header_buckets % txs_count %
        (to_double(txs_count) / header_buckets));

    for (const auto& entry: dump(txs))
        logger(format("txs: %1% frequency: %2%") %
            entry.first % entry.second);
 
    txs.clear();
    txs.shrink_to_fit();

    // tx & strong_tx (strong_tx is a proxy for validated_tx)
    // ------------------------------------------------------------------------

    index = max_size_t;
    start = logger::now();
    const auto tx_buckets = query_.tx_buckets();
    const auto tx_records = query_.tx_records();
    std_vector<size_t> tx(tx_buckets, empty);
    std_vector<size_t> strong_tx(tx_buckets, empty);
    while (!cancel_ && (++index < tx_records))
    {
        const tx_link link{ possible_narrow_cast<tx_link::integer>(index) };
        const auto key = query_.get_tx_key(link.value);
        ++tx.at(database::keys::hash(key) % tx_buckets);
        ++strong_tx.at(database::keys::hash(
            link.operator data_array<tx_link::size>()) % tx_buckets);
    
        if (is_zero(index % tx_frequency))
            logger(format("tx & strong_tx" BN_READ_ROW) % index %
                duration_cast<seconds>(logger::now() - start).count());
    }
    
    if (cancel_)
        logger(BN_OPERATION_CANCELED);
    
    // ........................................................................
    
    const auto tx_count = count(tx);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("tx: %1% in %2%s buckets %3% filled %4% rate %5%") %
        index % span.count() % tx_buckets % tx_count %
        (to_double(tx_count) / tx_buckets));
    
    for (const auto& entry: dump(tx))
        logger(format("tx: %1% frequency: %2%") %
            entry.first % entry.second);
    
    tx.clear();
    tx.shrink_to_fit();
    
    // ........................................................................
    
    const auto strong_tx_count = count(strong_tx);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("strong_tx: %1% in %2%s buckets %3% filled %4% rate %5%") %
        index % span.count() % tx_buckets % strong_tx_count %
        (to_double(strong_tx_count) / tx_buckets));
    
    for (const auto& entry: dump(strong_tx))
        logger(format("strong_tx: %1% frequency: %2%") %
            entry.first % entry.second);
    
    strong_tx.clear();
    strong_tx.shrink_to_fit();
    
    // point
    // ------------------------------------------------------------------------

    index = max_size_t;
    start = logger::now();
    const auto point_buckets = query_.point_buckets();
    std_vector<size_t> spend(point_buckets, empty);
    auto inserts = zero;

    // TODO: expose filter type from hashhead to table.
    ///////////////////////////////////////////////////////////////////////////
    constexpr size_t m = 32;
    constexpr size_t k = 6;
    using filter_t = bloom<m, k>;
    constexpr auto empty_filter = unmask_right<filter_t::type>(m);
    ///////////////////////////////////////////////////////////////////////////
    std_vector<filter_t::type> filter(point_buckets, empty_filter);
    size_t collisions{};
    size_t coinbases{};
    size_t subtotal{};
    size_t window{};

    while (!cancel_ && (++index < query_.header_records()))
    {
        ++coinbases;
        const header_link link{ possible_narrow_cast<hint>(index) };
        const auto transactions = query_.to_transactions(link);
        for (const auto& transaction: transactions)
        {
            const auto points = query_.to_points(transaction);
            for (const auto& point: points)
            {
                const auto key = query_.get_point(point);
                const auto bucket = database::keys::hash(key) % point_buckets;
                const auto entropy = database::keys::thumb(key);
                ++spend.at(bucket);
                ++inserts;

                const auto prev = filter.at(bucket);
                const auto next = filter_t::screen(prev, entropy);
                filter.at(bucket) = next;

                // All coinbases should collide with the first.
                const auto coll = to_int(filter_t::is_collision(prev, next));
                collisions += coll;
                subtotal += coll;
                ++window;

                if (is_zero(inserts % put_frequency))
                {
                    logger(format("point: %1% fps %2% rate %3$.7f in %4% secs.") %
                        inserts % collisions % (to_double(subtotal) / window) %
                        duration_cast<seconds>(logger::now() - start).count());

                    subtotal = zero;
                    window = zero;
                }
            }
        }
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    // ........................................................................

    const auto point_count = count(spend);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("point: %1% in %2%s buckets %3% filled %4% rate %5%") %
        inserts % span.count() % point_buckets % point_count %
        (to_double(point_count) / point_buckets));

    const auto spends = inserts - coinbases;
    const auto spend_collisions = collisions - coinbases;
    logger(format("bloom: %1% fps of %2% spends (ex %3% cbs) rate %4%") %
        spend_collisions % spends % coinbases %
        (to_double(spend_collisions) / spends));

    for (const auto& entry: dump(spend))
        logger(format("point: %1% frequency: %2%") %
            entry.first % entry.second);


    ////spend.clear();
    ////spend.shrink_to_fit();
    ////filter.clear();
    ////filter.shrink_to_fit();
}

} // namespace node
} // namespace libbitcoin
