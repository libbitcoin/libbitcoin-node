/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

// for do_hardware
#ifdef HAVE_XCPU
    constexpr auto with_xcpu = true;
#else
    constexpr auto with_xcpu = false;
#endif
#ifdef HAVE_ARM
    constexpr auto with_arm = true;
#else
    constexpr auto with_arm = false;
#endif

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace network;
using namespace system;
using namespace std::chrono;
using namespace std::placeholders;

// for --help only
const std::string executor::name_{ "bn" };

// for capture only
const std::string executor::close_{ "c" };

const std::unordered_map<std::string, uint8_t> executor::options_
{
    { "b", menu::backup },
    { "c", menu::close },
    { "e", menu::errors },
    { "g", menu::go },
    { "h", menu::hold },
    { "i", menu::info },
    { "t", menu::test },
    { "w", menu::work },
    { "z", menu::zeroize }
};
const std::unordered_map<uint8_t, std::string> executor::menu_
{
    { menu::backup,  "[b]ackup the store" },
    { menu::close,   "[c]lose the node" },
    { menu::errors,  "display any store [e]rrors" },
    { menu::go,      "[g]o network communication" },
    { menu::hold,    "[h]old network communication" },
    { menu::info,    "display node [i]nformation" },
    { menu::test,    "execute built-in [t]est" },
    { menu::work,    "display [w]ork distribution" },
    { menu::zeroize, "[z]eroize store disk full error" }
};
const std::unordered_map<std::string, uint8_t> executor::toggles_
{
    { "a", levels::application },
    { "n", levels::news },
    { "s", levels::session },
    { "p", levels::protocol },
    { "x", levels::proxy },
    { "w", levels::wire },
    { "r", levels::remote },
    { "f", levels::fault },
    { "q", levels::quit },
    { "o", levels::objects },
    { "v", levels::verbose }
};
const std::unordered_map<uint8_t, std::string> executor::display_
{
    { levels::application, "toggle Application" },
    { levels::news,        "toggle News" },
    { levels::session,     "toggle Session" },
    { levels::protocol,    "toggle Protocol" },
    { levels::proxy,       "toggle proXy" },
    { levels::wire,        "toggle Wire" }, // unused
    { levels::remote,      "toggle Remote" },
    { levels::fault,       "toggle Fault" },
    { levels::quit,        "toggle Quitting" },
    { levels::objects,     "toggle Objects" },
    { levels::verbose,     "toggle Verbose" }
};
const std::unordered_map<uint8_t, bool> executor::defined_
{
    { levels::application, true },
    { levels::news,        levels::news_defined },
    { levels::session,     levels::session_defined },
    { levels::protocol,    levels::protocol_defined },
    { levels::proxy,       levels::proxy_defined },
    { levels::wire,        levels::wire_defined },
    { levels::remote,      levels::remote_defined },
    { levels::fault,       levels::fault_defined },
    { levels::quit,        levels::quit_defined },
    { levels::objects,     levels::objects_defined },
    { levels::verbose,     levels::verbose_defined },
};
const std::unordered_map<uint8_t, std::string> executor::fired_
{
    { events::header_archived,     "header_archived....." },
    { events::header_organized,    "header_organized...." },
    { events::header_reorganized,  "header_reorganized.." },

    { events::block_archived,      "block_archived......" },
    { events::block_validated,     "block_validated....." },
    { events::block_confirmed,     "block_confirmed....." },
    { events::block_unconfirmable, "block_unconfirmable." },
    { events::block_malleated,     "block_malleated....." },
    { events::validate_bypassed,   "validate_bypassed..." },
    { events::confirm_bypassed,    "confirm_bypassed...." },

    { events::tx_archived,         "tx_archived........." },
    { events::tx_validated,        "tx_validated........" },
    { events::tx_invalidated,      "tx_invalidated......" },

    { events::block_organized,     "block_organized....." },
    { events::block_reorganized,   "block_reorganized..." },

    { events::template_issued,     "template_issued....." },
};
const std::unordered_map<database::event_t, std::string> executor::events_
{
    { database::event_t::create_file, "create_file" },
    { database::event_t::open_file,   "open_file" },
    { database::event_t::load_file, "load_file" },
    { database::event_t::unload_file, "unload_file" },
    { database::event_t::close_file, "close_file" },
    { database::event_t::create_table, "create_table" },
    { database::event_t::verify_table, "verify_table" },
    { database::event_t::close_table, "close_table" },

    { database::event_t::wait_lock, "wait_lock" },
    { database::event_t::flush_body, "flush_body" },
    { database::event_t::backup_table, "backup_table" },
    { database::event_t::copy_header, "copy_header" },
    { database::event_t::archive_snapshot, "archive_snapshot" },

    { database::event_t::restore_table, "restore_table" },
    { database::event_t::recover_snapshot, "recover_snapshot" }
};
const std::unordered_map<database::table_t, std::string> executor::tables_
{
    { database::table_t::store, "store" },

    { database::table_t::header_table, "header_table" },
    { database::table_t::header_head, "header_head" },
    { database::table_t::header_body, "header_body" },
    { database::table_t::point_table, "point_table" },
    { database::table_t::point_head, "point_head" },
    { database::table_t::point_body, "point_body" },
    { database::table_t::input_table, "input_table" },
    { database::table_t::input_head, "input_head" },
    { database::table_t::input_body, "input_body" },
    { database::table_t::output_table, "output_table" },
    { database::table_t::output_head, "output_head" },
    { database::table_t::output_body, "output_body" },
    { database::table_t::puts_table, "puts_table" },
    { database::table_t::puts_head, "puts_head" },
    { database::table_t::puts_body, "puts_body" },
    { database::table_t::tx_table, "tx_table" },
    { database::table_t::tx_head, "tx_head" },
    { database::table_t::txs_table, "txs_table" },
    { database::table_t::tx_body, "tx_body" },
    { database::table_t::txs_head, "txs_head" },
    { database::table_t::txs_body, "txs_body" },

    { database::table_t::address_table, "address_table" },
    { database::table_t::address_head, "address_head" },
    { database::table_t::address_body, "address_body" },
    { database::table_t::candidate_table, "candidate_table" },
    { database::table_t::candidate_head, "candidate_head" },
    { database::table_t::candidate_body, "candidate_body" },
    { database::table_t::confirmed_table, "confirmed_table" },
    { database::table_t::confirmed_head, "confirmed_head" },
    { database::table_t::confirmed_body, "confirmed_body" },
    { database::table_t::spend_table, "spend_table" },
    { database::table_t::spend_head, "spend_head" },
    { database::table_t::spend_body, "spend_body" },
    { database::table_t::strong_tx_table, "strong_tx_table" },
    { database::table_t::strong_tx_head, "strong_tx_head" },
    { database::table_t::strong_tx_body, "strong_tx_body" },

    { database::table_t::validated_bk_table, "validated_bk_table" },
    { database::table_t::validated_bk_head, "validated_bk_head" },
    { database::table_t::validated_bk_body, "validated_bk_body" },
    { database::table_t::validated_tx_table, "validated_tx_table" },
    { database::table_t::validated_tx_head, "validated_tx_head" },
    { database::table_t::validated_tx_body, "validated_tx_body" },
    { database::table_t::neutrino_table, "neutrino_table" },
    { database::table_t::neutrino_head, "neutrino_head" },
    { database::table_t::neutrino_body, "neutrino_body" }
    ////{ database::table_t::bootstrap_table, "bootstrap_table" },
    ////{ database::table_t::bootstrap_head, "bootstrap_head" },
    ////{ database::table_t::bootstrap_body, "bootstrap_body" },
    ////{ database::table_t::buffer_table, "buffer_table" },
    ////{ database::table_t::buffer_head, "buffer_head" },
    ////{ database::table_t::buffer_body, "buffer_body" }
};

// non-const member static (global for blocking interrupt handling).
std::promise<code> executor::stopping_{};

// non-const member static (global for non-blocking interrupt handling).
std::atomic_bool executor::cancel_{};

executor::executor(parser& metadata, std::istream& input, std::ostream& output,
    std::ostream&)
  : metadata_(metadata),
    store_(metadata.configured.database),
    query_(store_),
    input_(input),
    output_(output)
{
    // Turn of console echoing from std::cin to std:cout.
    system::unset_console_echo();

    // Capture <ctrl-c>.
    initialize_stop();
}

// Utility.
// ----------------------------------------------------------------------------

void executor::logger(const auto& message) const
{
    if (log_.stopped())
        output_ << message << std::endl;
    else
        log_.write(levels::application) << message << std::endl;
};

void executor::stopper(const auto& message)
{
    capture_.stop();
    log_.stop(message, levels::application);
    stopped_.get_future().wait();
}

// Measures.
// ----------------------------------------------------------------------------

void executor::dump_sizes() const
{
    logger(format(BN_MEASURE_SIZES) %
        query_.header_size() %
        query_.txs_size() %
        query_.tx_size() %
        query_.point_size() %
        query_.input_size() %
        query_.output_size() %
        query_.puts_size() %
        query_.candidate_size() %
        query_.confirmed_size() %
        query_.spend_size() %
        query_.strong_tx_size() %
        query_.validated_tx_size() %
        query_.validated_bk_size() %
        query_.address_size() %
        query_.neutrino_size());
}

void executor::dump_records() const
{
    logger(format(BN_MEASURE_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.spend_records() %
        query_.strong_tx_records() %
        query_.address_records());
}

void executor::dump_buckets() const
{
    logger(format(BN_MEASURE_BUCKETS) %
        query_.header_buckets() %
        query_.txs_buckets() %
        query_.tx_buckets() %
        query_.point_buckets() %
        query_.spend_buckets() %
        query_.strong_tx_buckets() %
        query_.validated_tx_buckets() %
        query_.validated_bk_buckets() %
        query_.address_buckets() %
        query_.neutrino_buckets());
}

void executor::dump_progress() const
{
    logger(format(BN_MEASURE_PROGRESS) %
        query_.get_fork() %
        query_.get_top_confirmed() %
        encode_hash(query_.get_header_key(query_.to_confirmed(
            query_.get_top_confirmed()))) %
        query_.get_top_candidate() %
        encode_hash(query_.get_header_key(query_.to_candidate(
            query_.get_top_candidate()))) %
        query_.get_top_associated() %
        (query_.get_top_candidate() - query_.get_unassociated_count()) %
        query_.get_confirmed_size() %
        query_.get_candidate_size());
}

// txs, validated_tx, validated_bk collision rates assume 1:1 records.
void executor::dump_collisions() const
{
    logger(format(BN_MEASURE_COLLISION_RATES) %
        ((1.0 * query_.header_records()) / query_.header_buckets()) %
        ((1.0 * query_.header_records()) / query_.txs_buckets()) %
        ((1.0 * query_.tx_records()) / query_.tx_buckets()) %
        ((1.0 * query_.point_records()) / query_.point_buckets()) %
        ((1.0 * query_.spend_records()) / query_.spend_buckets()) %
        ((1.0 * query_.strong_tx_records()) / query_.strong_tx_buckets()) %
        ((1.0 * query_.tx_records()) / query_.validated_tx_buckets()) %
        ((1.0 * query_.header_records()) / query_.validated_bk_buckets()) %
        (query_.address_enabled() ? ((1.0 * query_.address_records()) /
            query_.address_buckets()) : 0) %
        (query_.neutrino_enabled() ? ((1.0 * query_.header_records()) /
            query_.neutrino_buckets()) : 0));
}

// fork flag transitions (candidate chain).
void executor::scan_flags() const
{
    const auto start = logger::now();
    constexpr auto flag_bits = to_bits(sizeof(chain::flags));
    const auto error = code{ error::store_integrity }.message();
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

// file and logical sizes.
void executor::measure_size() const
{
    dump_sizes();
    dump_records();
    dump_buckets();
    dump_collisions();

    // This one can take a few seconds on cold iron.
    logger(BN_MEASURE_PROGRESS_START);
    dump_progress();
}

// input and output table slab counts.
void executor::scan_slabs() const
{
    logger(BN_MEASURE_SLABS);
    logger(BN_OPERATION_INTERRUPT);
    database::tx_link::integer link{};
    size_t inputs{}, outputs{};
    const auto start = fine_clock::now();
    constexpr auto frequency = 100'000;

    // Tx (record) links are sequential and so iterable, however the terminal
    // condition assumes all tx entries fully written (ok for stopped node).
    // A running node cannot safely iterate over record links, but stopped can.
    for (auto puts = query_.put_counts(link);
        to_bool(puts.first) && !cancel_.load();
        puts = query_.put_counts(++link))
    {
        inputs += puts.first;
        outputs += puts.second;
        if (is_zero(link % frequency))
            logger(format(BN_MEASURE_SLABS_ROW) % link % inputs % outputs);
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
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
    logger(format("header" BN_READ_ROW) % (1.0 * filled / bucket) %
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
    logger(format("txs" BN_READ_ROW) % (1.0 * filled / bucket) %
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
    logger(format("tx" BN_READ_ROW) % (1.0 * filled / bucket) %
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

        if (is_zero(bucket % tx_frequency))
            logger(format("point" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(format("point" BN_READ_ROW) % (1.0 * filled / bucket) %
        span.count());

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!cancel_ && (++bucket < query_.spend_buckets()))
    {
        const auto top = query_.top_spend(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % put_frequency))
            logger(format("spend" BN_READ_ROW) % bucket %
                duration_cast<seconds>(logger::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(format("spend" BN_READ_ROW) % (1.0 * filled / bucket) %
        span.count());
}

// hashmap collision distributions.
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
    constexpr auto floater = [](const auto value)
    {
        return 1.0 * value;
    };
    constexpr auto hash = [](const auto& key)
    {
        constexpr auto length = array_count<decltype(key)>;
        constexpr auto size = std::min(length, sizeof(size_t));
        size_t value{};
        std::copy_n(key.begin(), size, system::byte_cast(value).begin());
        return value;
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
        ++header.at(hash(query_.get_header_key(link.value)) % header_buckets);
        ++txs.at(hash((header_link::bytes)link) % header_buckets);

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
        (floater(header_count) / header_buckets));

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
        (floater(txs_count) / header_buckets));

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
        ++tx.at(hash(query_.get_tx_key(link.value)) % tx_buckets);
        ++strong_tx.at(hash((tx_link::bytes)link) % tx_buckets);
    
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
        (floater(tx_count) / tx_buckets));
    
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
        (floater(strong_tx_count) / tx_buckets));
    
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
    const auto point_records = query_.point_records();
    std_vector<size_t> point(point_buckets, empty);
    while (!cancel_ && (++index < point_records))
    {
        const tx_link link{ possible_narrow_cast<tx_link::integer>(index) };
        ++point.at(hash(query_.get_point_key(link.value)) % point_buckets);
    
        if (is_zero(index % tx_frequency))
            logger(format("point" BN_READ_ROW) % index %
                duration_cast<seconds>(logger::now() - start).count());
    }
    
    if (cancel_)
        logger(BN_OPERATION_CANCELED);
    
    // ........................................................................
    
    const auto point_count = count(point);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("point: %1% in %2%s buckets %3% filled %4% rate %5%") %
        index % span.count() % point_buckets % point_count %
        (floater(point_count) / point_buckets));
    
    for (const auto& entry: dump(point))
        logger(format("point: %1% frequency: %2%") %
            entry.first % entry.second);
    
    point.clear();
    point.shrink_to_fit();

    // spend
    // ------------------------------------------------------------------------

    auto total = zero;
    index = max_size_t;
    start = logger::now();
    const auto spend_buckets = query_.spend_buckets();
    std_vector<size_t> spend(spend_buckets, empty);
    while (!cancel_ && (++index < query_.header_records()))
    {
        const header_link link{ possible_narrow_cast<hint>(index) };
        const auto transactions = query_.to_txs(link);
        for (const auto& transaction: transactions)
        {
            const auto inputs = query_.to_tx_spends(transaction);
            for (const auto& in: inputs)
            {
                ++total;
                ++spend.at(hash(query_.to_spend_key(in)) % spend_buckets);

                if (is_zero(index % put_frequency))
                    logger(format("spend" BN_READ_ROW) % total %
                        duration_cast<seconds>(logger::now() - start).count());
            }
        }
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    // ........................................................................

    const auto spend_count = count(spend);
    span = duration_cast<seconds>(logger::now() - start);
    logger(format("spend: %1% in %2%s buckets %3% filled %4% rate %5%") %
        total % span.count() % spend_buckets % spend_count %
        (floater(spend_count) / spend_buckets));

    for (const auto& entry: dump(spend))
        logger(format("spend: %1% frequency: %2%") %
            entry.first % entry.second);

    spend.clear();
    spend.shrink_to_fit();
}

// arbitrary testing (const).
void executor::read_test() const
{
    logger("No read test implemented.");
}

#if defined(UNDEFINED)

void executor::read_test() const
{
    logger("Wire size computation.");
    const auto start = fine_clock::now();
    constexpr auto last = 500'000_size;

    size_t size{};
    for (auto height = zero; !cancel_ && height <= last; ++height)
    {
        const auto link = query_.to_candidate(height);
        if (link.is_terminal())
        {
            logger(format("Max candidate height is (%1%).") % sub1(height));
            return;
        }

        const auto bytes = query_.get_block_size(link);
        if (is_zero(bytes))
        {
            logger(format("Block (%1%) is not associated.") % height);
            return;
        }

        size += bytes;
    }

    const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Wire size (%1%) at (%2%) in (%3%) ms.") %
        size % last % span.count());
}

void executor::read_test() const
{
    constexpr auto start_tx = 15'000_u32;
    constexpr auto target_count = 3000_size;

    // Set ensures unique addresses.
    std::set<hash_digest> keys{};
    auto tx = start_tx;

    logger(format("Getting first [%1%] output address hashes.") %
        target_count);

    auto start = fine_clock::now();
    while (!cancel_ && keys.size() < target_count)
    {
        const auto outputs = query_.get_outputs(tx++);
        if (outputs->empty())
            return;

        for (const auto& put: *outputs)
        {
            keys.emplace(put->script().hash());
            if (cancel_ || keys.size() == target_count)
                break;
        }
    }

    auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Got first [%1%] unique addresses above tx [%2%] in [%3%] ms.") %
        keys.size() % start_tx % span.count());

    struct out
    {
        // Address hash, output link, first spender link.
        hash_digest address;
        uint64_t output_fk;
        uint64_t spend_fk;
        uint64_t input_fk;

        // Output's tx link, hash, and block position.
        uint64_t tx_fk;
        hash_digest tx_hash;
        uint16_t tx_position;

        // Output tx's block link, hash, and height.
        uint32_t bk_fk;
        hash_digest bk_hash;
        uint32_t bk_height;

        // Spender's tx link, hash, and block position.
        uint64_t in_tx_fk;
        hash_digest in_tx_hash;
        uint16_t in_tx_position;

        // Spender tx's block link, hash, and height.
        uint32_t in_bk_fk;
        hash_digest in_bk_hash;
        uint32_t in_bk_height;

        // Spender (first input) and output.
        chain::output::cptr output{};
        chain::input::cptr input{};
    };

    std::vector<out> outs{};
    outs.reserve(target_count);
    using namespace database;

    start = fine_clock::now();
    for (auto& key: keys)
    {
        auto address_it = store_.address.it(key);
        if (cancel_ || address_it.self().is_terminal())
            return;

        do
        {
            table::address::record address{};
            if (cancel_ || !store_.address.get(address_it.self(), address))
                return;

            const auto out_fk = address.output_fk;
            table::output::get_parent output{};
            if (!store_.output.get(out_fk, output))
                return;

            const auto tx_fk = output.parent_fk;
            const auto block_fk = query_.to_block(tx_fk);

            // Output tx block has height.
            database::height_link bk_height{};
            table::header::get_height bk_header{};
            if (!block_fk.is_terminal())
            {
                if (!store_.header.get(block_fk, bk_header))
                    return;
                else
                    bk_height = bk_header.height;
            }

            // The block is confirmed by height.
            table::height::record height_record{};
            const auto confirmed = store_.confirmed.get(bk_height,
                height_record) && (height_record.header_fk == block_fk);

            // unconfirmed tx: max height/pos, null hash, terminal/zero links.
            if (!confirmed)
            {
                outs.push_back(out
                {
                    key,
                    out_fk,
                    max_uint64,  // spend_fk
                    max_uint64,  // input_fk

                    tx_fk,
                    null_hash,
                    max_uint16,  // position

                    block_fk,
                    null_hash,
                    max_uint32,  // height

                    // in_tx
                    max_uint64,
                    null_hash,
                    max_uint16,  // position

                    // in_bk_tx
                    max_uint32,
                    null_hash,
                    max_uint32,  // height

                    nullptr,
                    nullptr
                });
                continue;
            }

            // Get confirmed output tx block position.
            auto out_position = max_uint16;
            table::txs::get_position txs{ {}, tx_fk };
            if (!store_.txs.get(query_.to_txs_link(block_fk), txs))
                return;
            else
                out_position = possible_narrow_cast<uint16_t>(txs.position);

            // Get first spender only (may or may not be confirmed).
            const auto spenders = query_.to_spenders(out_fk);
            spend_link sp_fk{};
            if (!spenders.empty())
                sp_fk = spenders.front();

            // Get spender input, tx, position, block, height.
            auto in_position = max_uint16;
            input_link in_fk{};
            tx_link in_tx_fk{};
            header_link in_bk_fk{};
            height_link in_bk_height{};

            if (!sp_fk.is_terminal())
            {
                table::spend::record spend{};
                if (!store_.spend.get(sp_fk, spend))
                {
                    return;
                }
                else
                {
                    in_fk = spend.input_fk;
                    in_tx_fk = spend.parent_fk;
                }

                // Get spender tx block.
                in_bk_fk = query_.to_block(in_tx_fk);

                // Get in_tx position in the confirmed block.
                table::txs::get_position in_txs{ {}, in_tx_fk };
                if (!in_bk_fk.is_terminal())
                {
                    if (!store_.txs.get(query_.to_txs_link(in_bk_fk), in_txs))
                        return;
                    else
                        in_position = possible_narrow_cast<uint16_t>(in_txs.position);
                }

                // Get spender input tx block height.
                table::header::get_height in_bk_header{};
                if (!in_bk_fk.is_terminal())
                {
                    if (!store_.header.get(in_bk_fk, in_bk_header))
                        return;
                    else
                        in_bk_height = in_bk_header.height;
                }
            }

            // confirmed tx has block height and tx position.
            outs.push_back(out
            {
                key,
                out_fk,
                sp_fk,
                in_fk,

                tx_fk,
                query_.get_tx_key(tx_fk),
                out_position,

                block_fk,
                query_.get_header_key(block_fk),
                bk_height,

                in_tx_fk,
                query_.get_tx_key(in_tx_fk),
                in_position,

                in_bk_fk,
                query_.get_header_key(in_bk_fk),
                in_bk_height,

                query_.get_output(out_fk),
                query_.get_input(sp_fk)
            });
        }
        while (address_it.advance());
    }

    span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Got all [%1%] payments to [%2%] addresses in [%3%] ms.") %
        outs.size() % keys.size() % span.count());

    ////logger(
    ////    "output_script_hash, "
    ////    "output_fk, "
    ////    "spend_fk, "
    ////    "input_fk, "
    ////
    ////    "ouput_tx_fk, "
    ////    "ouput_tx_hash, "
    ////    "ouput_tx_pos, "
    ////
    ////    "ouput_bk_fk, "
    ////    "ouput_bk_hash, "
    ////    "ouput_bk_height, "
    ////
    ////    "input_tx_fk, "
    ////    "input_tx_hash, "
    ////    "input_tx_pos, "
    ////
    ////    "input_bk_fk, "
    ////    "input_bk_hash, "
    ////    "sinput_bk_height, "
    ////
    ////    "output_script "
    ////    "input_script, "
    ////);
    ////
    ////for (const auto& row: outs)
    ////{
    ////    if (cancel_) break;
    ////
    ////    const auto input = !row.input ? "{unspent}" :
    ////        row.input->script().to_string(chain::flags::all_rules);
    ////
    ////    const auto output = !row.output ? "{error}" :
    ////        row.output->script().to_string(chain::flags::all_rules);
    ////
    ////    logger(format("%1%, %2%, %3%, %4%, %5%, %6%, %7%, %8%, %9%, %10%, %11%, %12%, %13%, %14%, %15%, %16%, %17%, %18%") %
    ////        encode_hash(row.address) %
    ////        row.output_fk %
    ////        row.spend_fk%
    ////        row.input_fk%
    ////
    ////        row.tx_fk %
    ////        encode_hash(row.tx_hash) %
    ////        row.tx_position %
    ////
    ////        row.bk_fk %
    ////        encode_hash(row.bk_hash) %
    ////        row.bk_height %
    ////
    ////        row.in_tx_fk %
    ////        encode_hash(row.in_tx_hash) %
    ////        row.in_tx_position %
    ////
    ////        row.in_bk_fk %
    ////        encode_hash(row.in_bk_hash) %
    ////        row.in_bk_height %
    ////    
    ////        output%
    ////        input);
    ////}
}

void executor::read_test() const
{
    // Binance wallet address with 1,380,169 transaction count.
    // blockstream.info/address/bc1qm34lsc65zpw79lxes69zkqmk6ee3ewf0j77s3h
    const auto data = base16_array("0014dc6bf86354105de2fcd9868a2b0376d6731cb92f");
    const chain::script output_script{ data, false };
    const auto mnemonic = output_script.to_string(chain::flags::all_rules);
    logger(format("Getting payments to {%1%}.") % mnemonic);

    const auto start = fine_clock::now();
    database::output_links outputs{};
    if (!query_.to_address_outputs(outputs, output_script.hash()))
        return;

    const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Found [%1%] outputs of {%2%} in [%3%] ms.") %
        outputs.size() % mnemonic % span.count());
}

// This was caused by concurrent redundant downloads at tail following restart.
// The earlier transactions were marked as confirmed and during validation the
// most recent are found via point.hash association priot to to_block() test.
void executor::read_test() const
{
    const auto height = 839'287_size;
    const auto block = query_.to_confirmed(height);
    if (block.is_terminal())
    {
        logger("!block");
        return;
    }

    const auto txs = query_.to_txs(block);
    if (txs.empty())
    {
        logger("!txs");
        return;
    }

    database::tx_link spender_link{};
    const auto hash_spender = system::base16_hash("1ff970ec310c000595929bd290bbc8f4603ee18b2b4e3239dfb072aaca012b28");
    for (auto position = zero; !cancel_ && position < txs.size(); ++position)
    {
        const auto temp = txs.at(position);
        if (query_.get_tx_key(temp) == hash_spender)
        {
            spender_link = temp;
            break;
        }
    }

    auto spenders = store_.tx.it(hash_spender);
    if (spenders.self().is_terminal())
        return;

    // ...260, 261
    size_t spender_count{};
    do
    {
        const auto foo = spenders.self();
        ++spender_count;
    } while(spenders.advance());

    if (is_zero(spender_count))
    {
        logger("is_zero(spender_count)");
        return;
    }

    // ...260
    if (spender_link.is_terminal())
    {
        logger("spender_link.is_terminal()");
        return;
    }

    const auto spender_link1 = query_.to_tx(hash_spender);
    if (spender_link != spender_link1)
    {
        logger("spender_link != spender_link1");
        ////return;
    }

    database::tx_link spent_link{};
    const auto hash_spent = system::base16_hash("85f65b57b88b74fd945a66a6ba392a5f3c8a7c0f78c8397228dece885d788841");
    for (auto position = zero; !cancel_ && position < txs.size(); ++position)
    {
        const auto temp = txs.at(position);
        if (query_.get_tx_key(temp) == hash_spent)
        {
            spent_link = temp;
            break;
        }
    }

    auto spent = store_.tx.it(hash_spent);
    if (spent.self().is_terminal())
        return;

    // ...255, 254
    size_t spent_count{};
    do
    {
        const auto bar = spent.self();
        ++spent_count;
    } while (spent.advance());

    if (is_zero(spent_count))
    {
        logger("is_zero(spent_count)");
        return;
    }

    // ...254 (not ...255)
    if (spent_link.is_terminal())
    {
        logger("spent_link.is_terminal()");
        return;
    }

    const auto spent_link1 = query_.to_tx(hash_spent);
    if (spent_link != spent_link1)
    {
        logger("spent_link != spent_link1");
        ////return;
    }

    const auto tx = query_.to_tx(hash_spender);
    if (tx.is_terminal())
    {
        logger("!tx");
        return;
    }

    if (tx != spender_link)
    {
        logger("tx != spender_link");
        return;
    }

    if (spender_link <= spent_link)
    {
        logger("spender_link <= spent_link");
        return;
    }

    // ...254
    const auto header1 = query_.to_block(spender_link);
    if (header1.is_terminal())
    {
        logger("header1.is_terminal()");
        return;
    }

    // ...255 (the latter instance is not confirmed)
    const auto header11 = query_.to_block(add1(spender_link));
    if (!header11.is_terminal())
    {
        logger("!header11.is_terminal()");
        return;
    }

    // ...260
    const auto header2 = query_.to_block(spent_link);
    if (header2.is_terminal())
    {
        logger("auto.is_terminal()");
        return;
    }

    // ...261 (the latter instance is not confirmed)
    const auto header22 = query_.to_block(add1(spent_link));
    if (!header22.is_terminal())
    {
        logger("!header22.is_terminal()");
        return;
    }

    if (header1 != header2)
    {
        logger("header1 != header2");
        return;
    }

    if (header1 != block)
    {
        logger("header1 != block");
        return;
    }

    const auto ec = query_.block_confirmable(query_.to_confirmed(height));
    logger(format("Confirm [%1%] test (%2%).") % height % ec.message());
}

void executor::read_test() const
{
    const auto bk_link = query_.to_candidate(804'001_size);
    const auto block = query_.get_block(bk_link);
    if (!block)
    {
        logger("!query_.get_block(link)");
        return;
    }

    ////const auto tx = query_.get_transaction({ 980'984'671_u32 });
    ////if (!tx)
    ////{
    ////    logger("!query_.get_transaction(tx_link)");
    ////    return;
    ////}
    ////
    ////database::context context{};
    ////if (!query_.get_context(context, bk_link))
    ////{
    ////    logger("!query_.get_context(context, bk_link)");
    ////    return;
    ////}
    ////
    ////const chain::context ctx
    ////{
    ////    context.flags,
    ////    {},
    ////    context.mtp,
    ////    context.height,
    ////    {},
    ////    {}
    ////};
    ////
    ////if (!query_.populate_with_metadata(*tx))
    ////{
    ////    logger("!query_.populate_with_metadata(*tx)");
    ////    return;
    ////}
    ////
    ////if (const auto ec = tx->confirm(ctx))
    ////    logger(format("Error confirming tx [980'984'671] %1%") % ec.message());
    ////
    ////// Does not compute spent metadata, assumes coinbase spent and others not.
    ////if (!query_.populate_with_metadata(*block))
    ////{
    ////    logger("!query_.populate_with_metadata(*block)");
    ////    return;
    ////}
    ////
    ////const auto& txs = *block->transactions_ptr();
    ////if (txs.empty())
    ////{
    ////    logger("txs.empty()");
    ////    return;
    ////}
    ////
    ////for (auto index = one; index < txs.size(); ++index)
    ////    if (const auto ec = txs.at(index)->confirm(ctx))
    ////        logger(format("Error confirming tx [%1%] %2%") % index % ec.message());
    ////
    ////logger("Confirm test 1 complete.");

    const auto ec = query_.block_confirmable(bk_link);
    logger(format("Confirm test 2 complete (%1%).") % ec.message());
}

void executor::read_test() const
{
    using namespace database;
    constexpr auto frequency = 100'000u;
    const auto start = fine_clock::now();
    auto tx = 664'400'000_size;

    // Read all data except genesis (ie. for validation).
    while (!cancel_ && (++tx < query_.tx_records()))
    {
        const tx_link link{
            system::possible_narrow_cast<tx_link::integer>(tx) };

        ////const auto ptr = query_.get_header(link);
        ////if (!ptr)
        ////{
        ////    logger("Failure: get_header");
        ////    break;
        ////}
        ////else if (is_zero(ptr->bits()))
        ////{
        ////    logger("Failure: zero bits");
        ////    break;
        ////}

        ////const auto txs = query_.to_txs(link);
        ////if (txs.empty())
        ////{
        ////    logger("Failure: to_txs");
        ////    break;
        ////}

        const auto ptr = query_.get_transaction(link);
        if (!ptr)
        {
            logger("Failure: get_transaction");
            break;
        }
        else if (!ptr->is_valid())
        {
            logger("Failure: is_valid");
            break;
        }

        if (is_zero(tx % frequency))
            logger(format("get_transaction" BN_READ_ROW) % tx %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }

    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("get_transaction" BN_READ_ROW) % tx % span.count());
}

void executor::read_test() const
{
    constexpr auto hash492224 = base16_hash(
        "0000000000000000003277b639e56dffe2b4e60d18aeedb1fe8b7e4256b2a526");

    logger("HIT <enter> TO START");
    std::string line{};
    std::getline(input_, line);
    const auto start = fine_clock::now();

    for (size_t height = 492'224; (height <= 492'224) && !cancel_; ++height)
    {
        // 2s 0s
        const auto link = query_.to_header(hash492224);
        if (link.is_terminal())
        {
            logger("to_header");
            return;
        }

        ////const auto link = query_.to_confirmed(height);
        ////if (link.is_terminal())
        ////{
        ////    logger("to_confirmed");
        ////    return;
        ////}

        // 109s 111s
        const auto block = query_.get_block(link);
        if (!block || !block->is_valid() || block->hash() != hash492224)
        {
            logger("get_block");
            return;
        }
        
        // 125s 125s
        code ec{};
        if ((ec = block->check()))
        {
            logger(format("Block [%1%] check1: %2%") % height % ec.message());
            return;
        }

        // 117s 122s
        if (chain::checkpoint::is_conflict(
            metadata_.configured.bitcoin.checkpoints, block->hash(), height))
        {
            logger(format("Block [%1%] checkpoint conflict") % height);
            return;
        }

        ////// ???? 125s/128s
        ////block->populate();

        // 191s 215s/212s/208s [independent]
        // ???? 228s/219s/200s [combined]
        if (!query_.populate(*block))
        {
            logger("populate");
            return;
        }

        // 182s
        database::context ctx{};
        if (!query_.get_context(ctx, link) || ctx.height != height)
        {
            logger("get_context");
            return;
        }

        // Fabricate chain_state context from store context.
        chain::context state{};
        state.flags = ctx.flags;
        state.height = ctx.height;
        state.median_time_past = ctx.mtp;
        state.timestamp = block->header().timestamp();

        // split from accept.
        if ((ec = block->check(state)))
        {
            logger(format("Block [%1%] check2: %2%") % height % ec.message());
            return;
        }

        // 199s
        const auto& coin = metadata_.configured.bitcoin;
        if ((ec = block->accept(state, coin.subsidy_interval_blocks,
            coin.initial_subsidy())))
        {
            logger(format("Block [%1%] accept: %2%") % height % ec.message());
            return;
        }

        // 1410s
        if ((ec = block->connect(state)))
        {
            logger(format("Block [%1%] connect: %2%") % height % ec.message());
            return;
        }

        ////for (size_t index = one; index < block->transactions_ptr()->size(); ++index)
        ////{
        ////    constexpr size_t index = 1933;
        ////    const auto& tx = *block->transactions_ptr()->at(index);
        ////    if ((ec = tx.connect(state)))
        ////    {
        ////        logger(format("Tx (%1%) [%2%] %3%")
        ////            % index
        ////            % encode_hash(tx.hash(false))
        ////            % ec.message());
        ////    }
        ////}

        // +10s for all.
        logger(format("block:%1%") % height);
        ////logger(format("block:%1% flags:%2% mtp:%3%") %
        ////    ctx.height % ctx.flags % ctx.mtp);
    }

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("STOP (%1% secs)") % span.count());
}

// TODO: create a block/tx dumper.
void executor::read_test() const
{
    constexpr auto hash251684 = base16_hash(
        "00000000000000720e4c59ad28a8b61f38015808e92465e53111e3463aed80de");
    constexpr auto hash9 = base16_hash(
        "61a078472543e9de9247446076320499c108b52307d8d0fafbe53b5c4e32acc4");

    const auto link = query_.to_header(hash251684);
    if (link.is_terminal())
    {
        logger("link.is_terminal()");
        return;
    }

    const auto block = query_.get_block(link);
    if (!block)
    {
        logger("!block");
        return;
    }
    if (!block->is_valid())
    {
        logger("!block->is_valid()");
        return;
    }

    database::context ctx{};
    if (!query_.get_context(ctx, link))
    {
        logger("!query_.get_context(ctx, link)");
        return;
    }

    // flags:131223 height:251684 mtp:1376283946
    logger(format("flags:%1% height:%2% mtp:%3%") %
        ctx.flags % ctx.height % ctx.mtp);

    // minimum_block_version and work_required are only for header validate.
    chain::context state{};
    state.flags = ctx.flags;
    state.height = ctx.height;
    state.median_time_past = ctx.mtp;
    state.timestamp = block->header().timestamp();
    state.minimum_block_version = 0;
    state.work_required = 0;
    if (!query_.populate(*block))
    {
        logger("!query_.populate(*block)");
        return;
    }

    code ec{};
    if ((ec = block->check()))
    {
        logger(format("Block check: %1%") % ec.message());
        return;
    }

    const auto& coin = metadata_.configured.bitcoin;
    if ((ec = block->accept(state, coin.subsidy_interval_blocks,
        coin.initial_subsidy())))
    {
        logger(format("Block accept: %1%") % ec.message());
        return;
    }

    if ((ec = block->connect(state)))
    {
        logger(format("Block connect: %1%") % ec.message());
        return;
    }

    logger("Validated block 251684.");
}

#endif // UNDEFINED

// arbitrary testing (non-const).
void executor::write_test()
{
    logger("No write test implemented.");
}

#if defined(UNDEFINED)

void executor::write_test()
{
    code ec{};
    size_t count{};
    const auto start = fine_clock::now();

    ////// This tx in block 840161 is not strong by block 840112 (weak prevout).
    ////const auto tx = "865d721037b0c995822367c41875593d7093d1bae412f3861ce471de3c07e180";
    ////const auto block = "00000000000000000002504eefed4dc72956aa941aa7b6defe893e261de6a636";
    ////const auto hash = system::base16_hash(block);
    ////if (!query_.push_candidate(query_.to_header(hash)))
    ////{
    ////    logger(format("!query_.push_candidate(query_.to_header(hash))"));
    ////    return;
    ////}

    for (size_t height = query_.get_fork();
        !cancel_ && !ec && height <= query_.get_top_associated_from(height);
        ++height, ++count)
    {
        const auto block = query_.to_candidate(height);
        if (!query_.set_strong(block))
        {
            logger(format("set_strong [%1%] fault.") % height);
            return;
        }

        ////if (height > 804'000_size)
        ////if (ec = query_.block_confirmable(block))
        ////{
        ////    logger(format("block_confirmable [%1%] fault (%2%).") % height % ec.message());
        ////    return;
        ////}
        ////
        ////if (!query_.set_block_confirmable(block, uint64_t{}))
        ////{
        ////    logger(format("set_block_confirmable [%1%] fault.") % height);
        ////    return;
        ////}

        if (!query_.push_confirmed(block))
        {
            logger(format("push_confirmed [%1%] fault.") % height);
            return;
        }

        if (is_zero(height % 100_size))
            logger(format("write_test [%1%].") % height);
    }

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("%1% blocks in %2% secs.") % count % span.count());
}

void executor::write_test()
{
    using namespace database;
    constexpr auto frequency = 10'000;
    const auto start = fine_clock::now();
    logger(BN_OPERATION_INTERRUPT);

    auto height = query_.get_top_candidate();
    while (!cancel_ && (++height < query_.header_records()))
    {
        // Assumes height is header link.
        const header_link link{ possible_narrow_cast<header_link::integer>(height) };

        if (!query_.push_confirmed(link))
        {
            logger("!query_.push_confirmed(link)");
            return;
        }

        if (!query_.push_candidate(link))
        {
            logger("!query_.push_candidate(link)");
            return;
        }

        if (is_zero(height % frequency))
            logger(format("block" BN_WRITE_ROW) % height %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }
            
    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("block" BN_WRITE_ROW) % height % span.count());
}

void executor::write_test()
{
    using namespace database;
    ////constexpr uint64_t fees = 99;
    constexpr auto frequency = 10'000;
    const auto start = fine_clock::now();
    code ec{};

    logger(BN_OPERATION_INTERRUPT);

    auto height = zero;//// query_.get_top_confirmed();
    const auto records = query_.header_records();
    while (!cancel_ && (++height < records))
    {
        // Assumes height is header link.
        const header_link link{ possible_narrow_cast<header_link::integer>(height) };

        if (!query_.set_strong(link))
        {
            // total sequential chain cost: 18.7 min (now 6.6).
            logger("Failure: set_strong");
            break;
        }
        else if ((ec = query_.block_confirmable(link)))
        {
            // must set_strong before each (no push, verifies non-use).
            logger(format("Failure: block_confirmed, %1%") % ec.message());
            break;
        }
        ////if (!query_.set_txs_connected(link))
        ////{
        ////    // total sequential chain cost: 21 min.
        ////    logger("Failure: set_txs_connected");
        ////    break;
        ////}
        ////if (!query_.set_block_confirmable(link, fees))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: set_block_confirmable");
        ////    break;
        ////    break;
        ////}
        ////else if (!query_.push_candidate(link))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: push_candidate");
        ////    break;
        ////}
        ////else if (!query_.push_confirmed(link))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: push_confirmed");
        ////    break;
        ////}

        if (is_zero(height % frequency))
            logger(format("block" BN_WRITE_ROW) % height %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }
    
    if (cancel_)
        logger(BN_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("block" BN_WRITE_ROW) % height % span.count());
}

void executor::write_test()
{
    constexpr auto hash251684 = base16_hash(
        "00000000000000720e4c59ad28a8b61f38015808e92465e53111e3463aed80de");
    const auto link = query_.to_header(hash251684);
    if (link.is_terminal())
    {
        logger("link.is_terminal()");
        return;
    }

    if (query_.confirmed_records() != 251684u)
    {
        logger("!query_.confirmed_records() != 251684u");
        return;
    }

    if (!query_.push_confirmed(link))
    {
        logger("!query_.push_confirmed(link)");
        return;
    }

    if (query_.confirmed_records() != 251685u)
    {
        logger("!query_.confirmed_records() != 251685u");
        return;
    }

    logger("Successfully confirmed block 251684.");
}

#endif // UNDEFINED

// Store functions.
// ----------------------------------------------------------------------------

bool executor::check_store_path(bool create) const
{
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
    {
        logger(BN_USING_DEFAULT_CONFIG);
    }
    else
    {
        logger(format(BN_USING_CONFIG_FILE) % configuration);
    }

    const auto& store = metadata_.configured.database.path;
    if (create)
    {
        logger(format(BN_INITIALIZING_CHAIN) % store);
        if (!database::file::create_directory(store))
        {
            logger(format(BN_INITCHAIN_EXISTS) % store);
            return false;
        }
    }
    else
    {
        if (!database::file::is_directory(store))
        {
            logger(format(BN_UNINITIALIZED_DATABASE) % store);
            return false;
        }
    }

    return true;
}

bool executor::create_store(bool details)
{
    logger(BN_INITCHAIN_CREATING);
    const auto start = logger::now();
    if (const auto ec = store_.create([&](auto event_, auto table)
    {
        if (details)
            logger(format(BN_CREATE) % events_.at(event_) % tables_.at(table));
    }))
    {
        logger(format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message());
        return false;
    }

    // Create and confirm genesis block (store invalid without it).
    logger(BN_INITCHAIN_DATABASE_INITIALIZE);
    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        logger(BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BN_INITCHAIN_CREATED) % span.count());
    return true;
}

// not timed or announced (generally fast)
code executor::open_store_coded(bool details)
{
    ////logger(BN_DATABASE_STARTING);
    if (const auto ec = store_.open([&](auto event_, auto table)
    {
        if (details)
            logger(format(BN_OPEN) % events_.at(event_) % tables_.at(table));
    }))
    {
        logger(format(BN_DATABASE_START_FAIL) % ec.message());
        return ec;
    }

    logger(BN_DATABASE_STARTED);
    return error::success;
}

bool executor::open_store(bool details)
{
    return !open_store_coded(details);
}

bool executor::close_store(bool details)
{
    logger(BN_DATABASE_STOPPING);
    const auto start = logger::now();
    if (const auto ec = store_.close([&](auto event_, auto table)
    {
        if (details)
            logger(format(BN_CLOSE) % events_.at(event_) % tables_.at(table));
    }))
    {
        logger(format(BN_DATABASE_STOP_FAIL) % ec.message());
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BN_DATABASE_TIMED_STOP) % span.count());
    return true;
}

bool executor::restore_store(bool details)
{
    logger(BN_RESTORING_CHAIN);
    const auto start = logger::now();
    if (const auto ec = store_.restore([&](auto event, auto table)
    {
        if (details)
            logger(format(BN_RESTORE) % events_.at(event) % tables_.at(table));
    }))
    {
        if (ec == database::error::flush_lock)
            logger(BN_RESTORE_MISSING_FLUSH_LOCK);
        else
            logger(format(BN_RESTORE_FAILURE) % ec.message());

        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BN_RESTORE_COMPLETE) % span.count());
    return true;
}

bool executor::backup_store(bool details)
{
    if (!node_)
    {
        logger(BN_NODE_UNAVAILABLE);
        return false;
    }

    logger(BN_NODE_BACKUP_STARTED);
    const auto start = logger::now();
    if (const auto ec = node_->snapshot([&](auto event, auto table)
    {
        if (details)
            logger(format(BN_BACKUP) % events_.at(event) % tables_.at(table));
    }))
    {
        logger(format(BN_NODE_BACKUP_FAIL) % ec.message());
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BN_NODE_BACKUP_COMPLETE) % span.count());
    return true;
}

// Command line options.
// ----------------------------------------------------------------------------

// --[h]elp
bool executor::do_help()
{
    log_.stop();
    printer help(metadata_.load_options(), name_, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
    return true;
}

// --har[d]ware
bool executor::do_hardware()
{
    // Intrinsics can be safely compiled for unsupported platforms.
    // The "try" functions exclude out checks for instructions not compiled in.
    // But for instructions comiled in, each use currently invokes the "try".
    // HOWEVER: win32 compiler vectorization config is tied to these options.
    // HOWEVER: The process will crash if those are compiled and not present.
    // So our options are portable, but related compiler optimizations are not.
    // And in that case this function cannot even be executed. So test for
    // avx512 or shani here (for example) and only after enable compiler opts.

    log_.stop();
    logger("Intrinsics...");
    logger(format("arm..... platform:%1%.") % with_arm);
    logger(format("intel... platform:%1%.") % with_xcpu);
    logger(format("avx512.. platform:%1% compiled:%2%.") % system::try_avx512() % with_avx512);
    logger(format("avx2.... platform:%1% compiled:%2%.") % system::try_avx2() % with_avx2);
    logger(format("sse41... platform:%1% compiled:%2%.") % system::try_sse41() % with_sse41);
    logger(format("shani... platform:%1% compiled:%2%.") % system::try_shani() % with_shani);
    logger(format("neon.... platform:%1% compiled:%2%.") % system::try_neon() % with_neon);
    return true;
}

// --[s]ettings
bool executor::do_settings()
{
    log_.stop();
    printer print(metadata_.load_settings(), name_, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
    return true;
}

// --[v]ersion
bool executor::do_version()
{
    log_.stop();
    logger(format(BN_VERSION_MESSAGE)
        % LIBBITCOIN_NODE_VERSION
        % LIBBITCOIN_DATABASE_VERSION
        % LIBBITCOIN_NETWORK_VERSION
        % LIBBITCOIN_SYSTEM_VERSION);
    return true;
}

// --[i]nitchain
bool executor::do_initchain()
{
    log_.stop();
    if (!check_store_path(true) ||
        !create_store(true))
        return false;

    // Records and sizes reflect genesis block only.
    dump_sizes();
    dump_records();
    dump_buckets();

    if (!close_store())
        return false;

    logger(BN_INITCHAIN_COMPLETE);
    return true;
}

// --[b]ackup
bool executor::do_backup()
{
    log_.stop();
    return check_store_path()
        && open_store()
        && backup_store(true)
        && close_store();
}

// --restore[x]
bool executor::do_restore()
{
    log_.stop();
    return check_store_path()
        && restore_store(true)
        && close_store();
}

// --[f]lags
bool executor::do_flags()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_flags();
    return close_store();
}

// --[m]easure
bool executor::do_measure()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    measure_size();
    return close_store();
}

// --sl[a]bs
bool executor::do_slabs()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_slabs();
    return close_store();
}

// --buc[k]ets
bool executor::do_buckets()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_buckets();
    return close_store();
}

// --collisions[l]
bool executor::do_collisions()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_collisions();
    return close_store();
}

// --read[r]
bool executor::do_read()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    read_test();
    return close_store();
}

// --write[w]
bool executor::do_write()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    write_test();
    return close_store();
}

// Runtime options.
// ----------------------------------------------------------------------------

// [b]ackup
void executor::do_hot_backup()
{
    if (const auto ec = store_.get_fault())
    {
        logger(format(BN_SNAPSHOT_INVALID) % ec.message());
        return;
    }

    backup_store(true);
}

// [c]lose
void executor::do_close()
{
    logger("CONSOLE: Close");
    stop(error::success);
}

// [e]rrors
void executor::do_report_condition() const
{
    store_.report_errors([&](const auto& ec, auto table)
    {
        logger(format(BN_CONDITION) % tables_.at(table) % ec.message());
    });
}

// [h]old
void executor::do_suspend()
{
    if (!node_)
    {
        logger(BN_NODE_UNAVAILABLE);
        return;
    }

    node_->suspend(error::suspended_service);
}

// [g]o
void executor::do_resume()
{
    if (query_.is_full())
    {
        logger(BN_NODE_DISK_FULL);
        return;
    }

    if (!node_)
    {
        logger(BN_NODE_UNAVAILABLE);
        return;
    }

    node_->resume();
}

// [i]nformation
void executor::do_information() const
{
    dump_sizes();
    dump_records();
    dump_buckets();
    dump_collisions();
    ////dump_progress();
}

// [t]est
void executor::do_test() const
{
    read_test();
}

// [w]ork
void executor::do_report_work()
{
    if (!node_)
    {
        logger(BN_NODE_UNAVAILABLE);
        return;
    }

    logger(BN_NODE_REPORT_WORK);
    node_->notify(error::success, chase::report, work_counter_++);
}

// [z]eroize
void executor::do_reset_store()
{
    // Use do_resume command to restart connections after resetting here.
    if (query_.is_full())
    {
        if (!node_)
        {
            logger(BN_NODE_UNAVAILABLE);
            return;
        }

        store_.clear_errors();
        logger(BN_NODE_DISK_FULL_RESET);
        return;
    }

    // Any table with any error code.
    logger(query_.is_fault() ? BN_NODE_UNRECOVERABLE : BN_NODE_OK);
}

// Command line menu selection.
// ----------------------------------------------------------------------------

bool executor::menu()
{
    const auto& config = metadata_.configured;
    if (config.help)
        return do_help();

    // Order below matches help output (alphabetical), so that first option is
    // executed in the case where multiple options are parsed.

    if (config.slabs)
        return do_slabs();

    if (config.backup)
        return do_backup();

    if (config.hardware)
        return do_hardware();

    if (config.flags)
        return do_flags();

    if (config.initchain)
        return do_initchain();

    if (config.buckets)
        return do_buckets();

    if (config.collisions)
        return do_collisions();

    if (config.measure)
        return do_measure();

    if (config.read)
        return do_read();

    if (config.settings)
        return do_settings();

    if (config.version)
        return do_version();

    if (config.write)
        return do_write();

    if (config.restore)
        return do_restore();

    return do_run();
}

// Run.
// ----------------------------------------------------------------------------

executor::rotator_t executor::create_log_sink() const
{
    return
    {
        // Standard file names, within the [node].path directory.
        metadata_.configured.log.log_file1(),
        metadata_.configured.log.log_file2(),
        to_half(metadata_.configured.log.maximum_size)
    };
}

system::ofstream executor::create_event_sink() const
{
    // Standard file name, within the [node].path directory.
    return { metadata_.configured.log.events_file() };
}

void executor::subscribe_log(std::ostream& sink)
{
    log_.subscribe_messages([&](const code& ec, uint8_t level, time_t time,
        const std::string& message)
    {
        // Write only selected logs.
        if (!ec && !toggle_.at(level))
            return true;

        const auto prefix = format_zulu_time(time) + "." +
            serialize(level) + " ";

        if (ec)
        {
            sink << prefix << message << std::endl;
            output_ << prefix << message << std::endl;
            sink << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_FOOTER << std::endl;
            output_ << prefix << BN_NODE_TERMINATE << std::endl;
            stopped_.set_value(ec);
            return false;
        }
        else
        {
            sink << prefix << message;
            output_ << prefix << message;
            output_.flush();
            return true;
        }
    });
}

void executor::subscribe_events(std::ostream& sink)
{
    log_.subscribe_events([&sink, start = logger::now()](const code& ec,
        uint8_t event_, uint64_t value, const logger::time& point)
    {
        if (ec) return false;
        const auto time = duration_cast<seconds>(point - start).count();
        sink << fired_.at(event_) << " " << value << " " << time << std::endl;
        return true;
    });
}

// Runtime menu selection.
void executor::subscribe_capture()
{
    // This is not on a network thread, so the node may call close() while this
    // is running a backup (for example), resulting in a try_lock warning loop.
    capture_.subscribe([&](const code& ec, const std::string& line)
    {
        const auto token = system::trim_copy(line);

        // <control>-c emits empty token on Win32.
        if (token.empty())
            return true;

        if (token == "1")
        {
            for (const auto& option: menu_)
                logger(format("Option: %1%") % option.second);

            return true;
        }

        if (options_.contains(token))
        {
            switch (options_.at(token))
            {
                case menu::backup:
                {
                    do_hot_backup();
                    return true;
                }
                case menu::close:
                {
                    do_close();
                    return false;
                }
                case menu::errors:
                {
                    do_report_condition();
                    return true;
                }
                case menu::go:
                {
                    do_resume();
                    return true;
                }
                case menu::hold:
                {
                    do_suspend();
                    return true;
                }
                case menu::info:
                {
                    do_information();
                    return true;
                }
                case menu::test:
                {
                    do_test();
                    return true;
                }
                case menu::work:
                {
                    do_report_work();
                    return true;
                }
                case menu::zeroize:
                {
                    do_reset_store();
                    return true;
                }
                default:
                {
                    logger("CONSOLE: Unexpected option.");
                    return true;
                }
            }
        }

        if (toggles_.contains(token))
        {
            const auto toggle = toggles_.at(token);
            if (defined_.at(toggle))
            {
                toggle_.at(toggle) = !toggle_.at(toggle);
                logger("CONSOLE: " + display_.at(toggle) + (toggle_.at(toggle) ?
                    " logging (+)." : " logging (-)."));
            }
            else
            {
                // Selected log level was not compiled.
                logger("CONSOLE: " + display_.at(toggle) + " logging (~).");
            }

            return true;
        }

        logger("CONSOLE: '" + line + "'");
        return !ec;
    },
    [&](const code&)
    {
        // subscription completion handler.
    });
}

void executor::subscribe_connect()
{
    node_->subscribe_connect([&](const code&, const channel::ptr&)
    {
        log_.write(levels::verbose) <<
            "{in:" << node_->inbound_channel_count() << "}"
            "{ch:" << node_->channel_count() << "}"
            "{rv:" << node_->reserved_count() << "}"
            "{nc:" << node_->nonces_count() << "}"
            "{ad:" << node_->address_count() << "}"
            "{ss:" << node_->stop_subscriber_count() << "}"
            "{cs:" << node_->connect_subscriber_count() << "}."
            << std::endl;

        return true;
    },
    [&](const code&, uintptr_t)
    {
        // By not handling it is possible stop could fire before complete.
        // But the handler is not required for termination, so this is ok.
        // The error code in the handler can be used to differentiate.
    });
}

void executor::subscribe_close()
{
    node_->subscribe_close([&](const code&)
    {
        log_.write(levels::verbose) <<
            "{in:" << node_->inbound_channel_count() << "}"
            "{ch:" << node_->channel_count() << "}"
            "{rv:" << node_->reserved_count() << "}"
            "{nc:" << node_->nonces_count() << "}"
            "{ad:" << node_->address_count() << "}"
            "{ss:" << node_->stop_subscriber_count() << "}"
            "{cs:" << node_->connect_subscriber_count() << "}."
            << std::endl;

        return false;
    },
    [&](const code&, size_t)
    {
        // By not handling it is possible stop could fire before complete.
        // But the handler is not required for termination, so this is ok.
        // The error code in the handler can be used to differentiate.
    });
}

void executor::dump_options() const
{
    logger(BN_NODE_INTERRUPT);
    logger(BN_LOG_TABLE_HEADER);
    logger(format("Application.. " BN_LOG_TABLE) % levels::application_defined % toggle_.at(levels::application));
    logger(format("News......... " BN_LOG_TABLE) % levels::news_defined % toggle_.at(levels::news));
    logger(format("Session...... " BN_LOG_TABLE) % levels::session_defined % toggle_.at(levels::session));
    logger(format("Protocol..... " BN_LOG_TABLE) % levels::protocol_defined % toggle_.at(levels::protocol));
    logger(format("ProXy........ " BN_LOG_TABLE) % levels::proxy_defined % toggle_.at(levels::proxy));
    logger(format("Wire......... " BN_LOG_TABLE) % levels::wire_defined % toggle_.at(levels::wire));
    logger(format("Remote....... " BN_LOG_TABLE) % levels::remote_defined % toggle_.at(levels::remote));
    logger(format("Fault........ " BN_LOG_TABLE) % levels::fault_defined % toggle_.at(levels::fault));
    logger(format("Quit......... " BN_LOG_TABLE) % levels::quit_defined % toggle_.at(levels::quit));
    logger(format("Object....... " BN_LOG_TABLE) % levels::objects_defined % toggle_.at(levels::objects));
    logger(format("Verbose...... " BN_LOG_TABLE) % levels::verbose_defined % toggle_.at(levels::verbose));
}

// ----------------------------------------------------------------------------

bool executor::do_run()
{
    if (!metadata_.configured.log.path.empty())
        database::file::create_directory(metadata_.configured.log.path);

    // Hold sinks in scope for the length of the run.
    auto log = create_log_sink();
    auto events = create_event_sink();
    if (!log || !events)
    {
        logger(BN_LOG_INITIALIZE_FAILURE);
        return false;
    }

    subscribe_log(log);
    subscribe_events(events);
    subscribe_capture();
    logger(BN_LOG_HEADER);

    if (check_store_path())
    {
        auto ec = open_store_coded(true);
        if ((ec == database::error::flush_lock) && !restore_store(true))
            ec = error::store_integrity;

        if (ec)
        {
            stopper(BN_NODE_STOPPED);
            return false;
        }
    }
    else if (!check_store_path(true) || !create_store(true))
    {
        stopper(BN_NODE_STOPPED);
        return false;
    }

    dump_sizes();
    dump_records();
    dump_buckets();
    logger(BN_MEASURE_PROGRESS_START);
    dump_progress();

    // Stopped by stopper.
    capture_.start();
    dump_options();

    // Create node.
    metadata_.configured.network.initialize();
    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);

    // Subscribe node.
    subscribe_connect();
    subscribe_close();

    // Start network.
    logger(BN_NETWORK_STARTING);
    node_->start(std::bind(&executor::handle_started, this, _1));

    // Wait on signal to stop node (<ctrl-c>).
    stopping_.get_future().wait();
    toggle_.at(levels::protocol) = false;
    logger(BN_NETWORK_STOPPING);

    // Stop network (if not already stopped by self).
    node_->close();

    // Sizes and records change, buckets don't.
    dump_sizes();
    dump_records();
    logger(BN_MEASURE_PROGRESS_START);
    dump_progress();

    if (!close_store(true))
    {
        stopper(BN_NODE_STOPPED);
        return false;
    }

    stopper(BN_NODE_STOPPED);
    return true; 
}

// ----------------------------------------------------------------------------

void executor::handle_started(const code& ec)
{
    if (ec)
    {
        if (ec == error::store_uninitialized)
            logger(format(BN_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.path);
        else
            logger(format(BN_NODE_START_FAIL) % ec.message());

        stop(ec);
        return;
    }

    logger(BN_NODE_STARTED);

    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_subscribed, this, _1, _2));
}

void executor::handle_subscribed(const code& ec, size_t)
{
    if (ec)
    {
        logger(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        logger(format(BN_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    logger(BN_NODE_RUNNING);
}

bool executor::handle_stopped(const code& ec)
{
    if (ec && ec != network::error::service_stopped)
        logger(format(BN_NODE_STOP_CODE) % ec.message());

    // Signal stop (simulates <ctrl-c>).
    stop(ec);
    return false;
}

// Stop signal.
// ----------------------------------------------------------------------------

void executor::initialize_stop() NOEXCEPT
{
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);
}

void executor::handle_stop(int)
{
    initialize_stop();
    stop(error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec)
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&]()
    {
        cancel_.store(true);
        stopping_.set_value(ec);
    });
}

} // namespace node
} // namespace libbitcoin
