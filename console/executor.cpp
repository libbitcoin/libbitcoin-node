/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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

#include <chrono>
#include <csignal>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

using boost::format;
using system::config::printer;
using namespace system;
using namespace network;
using namespace std::placeholders;
using namespace std::chrono;

// const executor statics
const std::string executor::quit_{ "q" };
const std::string executor::name_{ "bn" };
const std::unordered_map<uint8_t, bool> executor::defined_
{
    { levels::application, true },
    { levels::news,        levels::news_defined },
    { levels::objects,     levels::objects_defined },
    { levels::session,     levels::session_defined },
    { levels::protocol,    levels::protocol_defined },
    { levels::proxy,       levels::proxy_defined },
    { levels::wire,        levels::wire_defined },
    { levels::remote,      levels::remote_defined },
    { levels::fault,       levels::fault_defined },
    { levels::quit,        levels::quit_defined }
};
const std::unordered_map<uint8_t, std::string> executor::display_
{
    { levels::application, "toggle Application" },
    { levels::news,        "toggle News" },
    { levels::objects,     "toggle Objects" },
    { levels::session,     "toggle Session" },
    { levels::protocol,    "toggle Protocol" },
    { levels::proxy,       "toggle proXy" },
    { levels::wire,        "toggle Wire shark" }, // not implemented
    { levels::remote,      "toggle Remote fault" },
    { levels::fault,       "toggle internal Fault" },
    { levels::quit,        "Quit" }
};
const std::unordered_map<std::string, uint8_t> executor::keys_
{
    { "a", levels::application },
    { "n", levels::news },
    { "o", levels::objects },
    { "s", levels::session },
    { "p", levels::protocol },
    { "x", levels::proxy },
    { "w", levels::wire },
    { "r", levels::remote },
    { "f", levels::fault },
    { quit_, levels::quit }
};
const std::unordered_map<database::event_t, std::string> executor::events_
{
    { database::event_t::create_file, "create_file" },
    { database::event_t::open_file, "open_file" },
    { database::event_t::load_file, "load_file" },
    { database::event_t::unload_file, "unload_file" },
    { database::event_t::close_file, "close_file" },
    { database::event_t::create_table, "create_table" },
    { database::event_t::verify_table, "verify_table" },
    { database::event_t::close_table, "close_table" }
};
const std::unordered_map<database::table_t, std::string> executor::tables_
{
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
    { database::table_t::strong_tx_table, "strong_tx_table" },
    { database::table_t::strong_tx_head, "strong_tx_head" },
    { database::table_t::strong_tx_body, "strong_tx_body" },
    { database::table_t::bootstrap_table, "bootstrap_table" },
    { database::table_t::bootstrap_head, "bootstrap_head" },
    { database::table_t::bootstrap_body, "bootstrap_body" },
    { database::table_t::buffer_table, "buffer_table" },
    { database::table_t::buffer_head, "buffer_head" },
    { database::table_t::buffer_body, "buffer_body" },
    { database::table_t::neutrino_table, "neutrino_table" },
    { database::table_t::neutrino_head, "neutrino_head" },
    { database::table_t::neutrino_body, "neutrino_body" },
    { database::table_t::validated_bk_table, "validated_bk_table" },
    { database::table_t::validated_bk_head, "validated_bk_head" },
    { database::table_t::validated_bk_body, "validated_bk_body" },
    { database::table_t::validated_tx_table, "validated_tx_table" },
    { database::table_t::validated_tx_head, "validated_tx_head" },
    { database::table_t::validated_tx_body, "validated_tx_body" }
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
    log_.write(levels::application) << message << std::endl;
};

void executor::console(const auto& message) const
{
    output_ << message << std::endl;
};

void executor::stopper(const auto& message)
{
    cap_.stop();
    log_.stop(message, levels::application);
    stopped_.get_future().wait();
}

void executor::measure_size() const
{
    constexpr auto frequency = 100'000;

    console(format(BN_MEASURE_SIZES) %
        query_.header_size() %
        query_.txs_size() %
        query_.tx_size() %
        query_.point_size() %
        query_.input_size() %
        query_.output_size() %
        query_.puts_size() %
        query_.candidate_size() %
        query_.confirmed_size() %
        query_.strong_tx_size() %
        query_.validated_tx_size() %
        query_.validated_bk_size());
    console(format(BN_MEASURE_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.puts_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.strong_tx_records());

    console(BN_MEASURE_SLABS);
    console(BN_OPERATION_INTERRUPT);
    database::tx_link::integer link{};
    size_t inputs{}, outputs{};
    const auto start = unix_time();

    // Links are sequential and therefore iterable, however the terminal
    // condition assumes all tx entries fully written (ok for stopped node).
    // A running node cannot safely iterate over record links, but stopped can.
    for (auto puts = query_.put_slabs(link);
        to_bool(puts.first) && !cancel_.load();
        puts = query_.put_slabs(++link))
    {
        inputs += puts.first;
        outputs += puts.second;
        if (is_zero(link % frequency))
            console(format(BN_MEASURE_SLABS_ROW) % link % inputs % outputs);
    }

    if (cancel_) console(BN_OPERATION_CANCELED);
    console(format(BN_MEASURE_STOP) % inputs % outputs % (unix_time() - start));

    // txs, validated_tx, validated_bk are estimates - assume 1:1 records.
    const auto header = (1.0 * query_.header_records()) /
        query_.header_buckets();
    const auto txs = (1.0 * query_.header_records()) /
        query_.txs_buckets();
    const auto tx = (1.0 * query_.tx_records()) /
        query_.tx_buckets();
    const auto point = (1.0 * query_.point_records()) /
        query_.point_buckets();
    const auto input = (1.0 * inputs) /
        query_.input_buckets();
    const auto strong_tx = (1.0 * query_.strong_tx_records()) /
        query_.strong_tx_buckets();
    const auto validated_tx = (1.0 * query_.tx_records()) /
        query_.validated_tx_buckets();
    const auto validated_bk = (1.0 * query_.header_records()) /
        query_.validated_bk_buckets();
    console(format(BN_MEASURE_COLLISION_RATES) %
        query_.header_buckets() % header %
        query_.txs_buckets() % txs %
        query_.tx_buckets() % tx %
        query_.point_buckets() % point %
        query_.input_buckets() % input %
        query_.strong_tx_buckets() % strong_tx %
        query_.validated_tx_buckets() % validated_tx %
        query_.validated_bk_buckets() % validated_bk);
}

////void executor::read_test() const
////{
////    constexpr auto frequency = 10'000;
////    const auto start = unix_time();
////    database::header_link::integer link{};
////    size_t height{};
////    bool all{ true };
////
////    console(BN_OPERATION_INTERRUPT);
////    const auto count = query_.header_records();
////    for (; !cancel_.load() && height < count; ++link, ++height)
////    {
////        all &= query_.is_confirmable_block(link, height);
////        if (is_zero(height % frequency))
////            console(format("is_confirmable_block" BN_READ_ROW) %
////                height % (unix_time() - start) % all);
////    }
////
////    if (cancel_) console(BN_OPERATION_CANCELED);
////    console(format("is_confirmable_block" BN_READ_ROW) %
////        height % (unix_time() - start) % all);
////}

void executor::read_test() const
{
    constexpr auto hash481831 = base16_hash(
        "0000000000000000008c40ba590fe957fb31ba256c82ae3ea4c378f46e3aa9b3");

    ////console("HIT <enter> TO START");
    ////std::string line{};
    ////std::getline(input_, line);
    const auto start = unix_time();

    for (size_t height = 481'831; (height <= 481'831) && !cancel_; ++height)
    {
        // 2s
        const auto link = query_.to_header(hash481831);
        if (link.is_terminal())
        {
            console("to_confirmed");
            return;
        }

        // 109s
        const auto block = query_.get_block(link);
        if (!block || !block->is_valid() || block->hash() != hash481831)
        {
            console("get_block");
            return;
        }
        
        // 125s
        code ec{};
        if ((ec = block->check()))
        {
            console(format("Block [%1%] check: %2%") % height % ec.message());
            return;
        }

        // 117s
        if (chain::checkpoint::is_conflict(
            metadata_.configured.bitcoin.checkpoints, block->hash(), height))
        {
            console(format("Block [%1%] checkpoint conflict") % height);
            return;
        }

        // 191s
        if (!query_.populate(*block))
        {
            console("populate");
            return;
        }

        // 182s
        database::context ctx{};
        if (!query_.get_context(ctx, link) || ctx.height != height)
        {
            console("get_context");
            return;
        }

        // Fabricate chain_state context from store context.
        chain::context state{};
        state.forks = ctx.flags;
        state.height = ctx.height;
        state.median_time_past = ctx.mtp;
        state.timestamp = block->header().timestamp();

        // hack in bit0 late and _bit1(segwit) on schedule.
        state.forks |= (chain::forks::bip9_bit0_group | chain::forks::bip9_bit1_group);

        // 199s
        const auto& coin = metadata_.configured.bitcoin;
        if ((ec = block->accept(state, coin.subsidy_interval_blocks,
            coin.initial_subsidy())))
        {
            console(format("Block [%1%] accept: %2%") % height % ec.message());
            return;
        }

        // 1410s
        if ((ec = block->connect(state)))
        {
            console(format("Block [%1%] connect: %2%") % height % ec.message());
            return;
        }

        ////for (size_t index = one; index < block->transactions_ptr()->size(); ++index)
        ////{
        ////    constexpr size_t index = 1906;
        ////    const auto& tx = *block->transactions_ptr()->at(index);
        ////    if ((ec = tx.connect(state)))
        ////    {
        ////        console(format("Tx (%1%) [%2%][%3%] %4%")
        ////            % index
        ////            % encode_hash(tx.hash(false))
        ////            % encode_hash(tx.hash(true))
        ////            % ec.message());
        ////    }
        ////}

        // +10s for all.
        console(format("block:%1%") % height);
        ////console(format("block:%1% flags:%2% mtp:%3%") %
        ////    ctx.height % ctx.flags % ctx.mtp);
    }

    console(format("STOP (%1% secs)") % (unix_time() - start));
}

void executor::write_test()
{
    constexpr auto frequency = 10'000;
    const auto start = unix_time();
    database::tx_link::integer link{};
    size_t height{};
    bool all{ true };

    console(BN_OPERATION_INTERRUPT);
    const auto count = query_.tx_records();
    ////const auto count = query_.header_records();
    for (; !cancel_.load() && height < count; ++link, ++height)
    {
        ////all &= query_.set_block_confirmable(link, 42);
        all &= query_.set_tx_connected(link, { 1, 2, 3 }, 42, 99);
        if (is_zero(height % frequency))
            console(format("set_tx_connected" BN_WRITE_ROW) % height %
                (unix_time() - start) % all);
    }

    if (cancel_) console(BN_OPERATION_CANCELED);
    console(format("set_tx_connected" BN_WRITE_ROW) % height %
        (unix_time() - start) % all);
}

// TODO: create a block/tx dumper.
////void executor::read_test() const
////{
////    constexpr auto hash251684 = base16_hash(
////        "00000000000000720e4c59ad28a8b61f38015808e92465e53111e3463aed80de");
////    constexpr auto hash9 = base16_hash(
////        "61a078472543e9de9247446076320499c108b52307d8d0fafbe53b5c4e32acc4");
////
////    const auto link = query_.to_header(hash251684);
////    if (link.is_terminal())
////    {
////        console("link.is_terminal()");
////        return;
////    }
////
////    const auto block = query_.get_block(link);
////    if (!block)
////    {
////        console("!block");
////        return;
////    }
////    if (!block->is_valid())
////    {
////        console("!block->is_valid()");
////        return;
////    }
////
////    database::context ctx{};
////    if (!query_.get_context(ctx, link))
////    {
////        console("!query_.get_context(ctx, link)");
////        return;
////    }
////
////    // flags:131223 height:251684 mtp:1376283946
////    console(format("flags:%1% height:%2% mtp:%3%") %
////        ctx.flags % ctx.height % ctx.mtp);
////
////    // minimum_block_version and work_required are only for header validate.
////    chain::context state{};
////    state.forks = ctx.flags;
////    state.height = ctx.height;
////    state.median_time_past = ctx.mtp;
////    state.timestamp = block->header().timestamp();
////    state.minimum_block_version = 0;
////    state.work_required = 0;
////    if (!query_.populate(*block))
////    {
////        console("!query_.populate(*block)");
////        return;
////    }
////
////    code ec{};
////    if ((ec = block->check()))
////    {
////        console(format("Block check: %1%") % ec.message());
////        return;
////    }
////
////    const auto& coin = metadata_.configured.bitcoin;
////    if ((ec = block->accept(state, coin.subsidy_interval_blocks,
////        coin.initial_subsidy())))
////    {
////        console(format("Block accept: %1%") % ec.message());
////        return;
////    }
////
////    if ((ec = block->connect(state)))
////    {
////        console(format("Block connect: %1%") % ec.message());
////        return;
////    }
////
////    console("Validated block 251684.");
////}
////
////void executor::write_test()
////{
////    constexpr auto hash251684 = base16_hash(
////        "00000000000000720e4c59ad28a8b61f38015808e92465e53111e3463aed80de");
////    const auto link = query_.to_header(hash251684);
////    if (link.is_terminal())
////    {
////        console("link.is_terminal()");
////        return;
////    }
////
////    if (query_.confirmed_records() != 251684u)
////    {
////        console("!query_.confirmed_records() != 251684u");
////        return;
////    }
////
////    if (!query_.push_confirmed(link))
////    {
////        console("!query_.push_confirmed(link)");
////        return;
////    }
////
////    if (query_.confirmed_records() != 251685u)
////    {
////        console("!query_.confirmed_records() != 251685u");
////        return;
////    }
////
////    console("Successfully confirmed block 251684.");
////}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu()
{
    const auto& config = metadata_.configured;

    if (config.help)
        return do_help();

    if (config.settings)
        return do_settings();

    if (config.version)
        return do_version();

    if (config.measure)
        return do_measure();

    if (config.read)
        return do_read();

    if (config.write)
        return do_write();

    if (config.initchain)
        return do_initchain();

    return do_run();
}

// Command line options.
// ----------------------------------------------------------------------------

// --help
bool executor::do_help()
{
    log_.stop();
    printer help(metadata_.load_options(), name_, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
    return true;
}

// --settings
bool executor::do_settings()
{
    log_.stop();
    printer print(metadata_.load_settings(), name_, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
    return true;
}

// --version
bool executor::do_version()
{
    log_.stop();
    console(format(BN_VERSION_MESSAGE)
        % LIBBITCOIN_NODE_VERSION
        % LIBBITCOIN_BLOCKCHAIN_VERSION
        % LIBBITCOIN_DATABASE_VERSION
        % LIBBITCOIN_NETWORK_VERSION
        % LIBBITCOIN_SYSTEM_VERSION);
    return true;
}

// --initchain
bool executor::do_initchain()
{
    log_.stop();
    const auto start = logger::now();
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
        console(BN_USING_DEFAULT_CONFIG);
    else
        console(format(BN_USING_CONFIG_FILE) % configuration);

    const auto& store = metadata_.configured.database.path;
    console(format(BN_INITIALIZING_CHAIN) % store);
    if (!database::file::create_directory(store))
    {
        console(format(BN_INITCHAIN_EXISTS) % store);
        return false;
    }

    console(BN_INITCHAIN_CREATING);
    if (const auto ec = store_.create([&](auto event, auto table)
    {
        console(format(BN_CREATE) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message());
        return false;
    }

    console(BN_STORE_STARTING);
    if (const auto ec = store_.open([&](auto event, auto table)
    {
        console(format(BN_OPEN) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_INITCHAIN_DATABASE_OPEN_FAILURE) % ec.message());
        return false;
    }

    console(BN_INITCHAIN_DATABASE_INITIALIZE);
    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        console(BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        return false;
    }

    // Records and sizes reflect genesis block only.
    console(format(BN_MEASURE_SIZES) %
        query_.header_size() %
        query_.txs_size() %
        query_.tx_size() %
        query_.point_size() %
        query_.input_size() %
        query_.output_size() %
        query_.puts_size() %
        query_.candidate_size() %
        query_.confirmed_size() %
        query_.strong_tx_size() %
        query_.validated_tx_size() %
        query_.validated_bk_size());
    console(format(BN_MEASURE_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.puts_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.strong_tx_records());
    console(format(BN_MEASURE_BUCKETS) %
        query_.header_buckets() %
        query_.txs_buckets() %
        query_.tx_buckets() %
        query_.point_buckets() %
        query_.input_buckets() %
        query_.strong_tx_buckets() %
        query_.validated_tx_buckets() %
        query_.validated_bk_buckets());

    console(BN_STORE_STOPPING);
    if (const auto ec = store_.close([&](auto event, auto table)
    {
        console(format(BN_CLOSE) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_INITCHAIN_DATABASE_CLOSE_FAILURE) % ec.message());
        return false;
    }

    const auto span = duration_cast<milliseconds>(logger::now() - start);
    console(format(BN_INITCHAIN_COMPLETE) % span.count());
    return true;
}

// --measure
bool executor::do_measure()
{
    log_.stop();
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
        console(BN_USING_DEFAULT_CONFIG);
    else
        console(format(BN_USING_CONFIG_FILE) % configuration);

    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        console(format(BN_UNINITIALIZED_STORE) % store);
        return false;
    }

    // Open store.
    console(BN_STORE_STARTING);
    if (const auto ec = store_.open([&](auto event, auto table)
    {
        console(format(BN_OPEN) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_START_FAIL) % ec.message());
        return false;
    }

    measure_size();

    // Close store.
    console(BN_STORE_STOPPING);
    if (const auto ec = store_.close([&](auto event, auto table)
    {
        console(format(BN_CLOSE) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_STOP_FAIL) % ec.message());
        return false;
    }

    console(BN_STORE_STOPPED);
    return true;
}

// --read
bool executor::do_read()
{
    log_.stop();
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
        console(BN_USING_DEFAULT_CONFIG);
    else
        console(format(BN_USING_CONFIG_FILE) % configuration);

    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        console(format(BN_UNINITIALIZED_STORE) % store);
        return false;
    }

    // Open store.
    console(BN_STORE_STARTING);
    if (const auto ec = store_.open([&](auto, auto)
    {
        ////console(format(BN_OPEN) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_START_FAIL) % ec.message());
        return false;
    }

    read_test();

    // Close store.
    console(BN_STORE_STOPPING);
    if (const auto ec = store_.close([&](auto, auto)
    {
        ////console(format(BN_CLOSE) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_STOP_FAIL) % ec.message());
        return false;
    }

    console(BN_STORE_STOPPED);
    return true;
}

// --write[f]
bool executor::do_write()
{
    log_.stop();
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
        console(BN_USING_DEFAULT_CONFIG);
    else
        console(format(BN_USING_CONFIG_FILE) % configuration);

    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        console(format(BN_UNINITIALIZED_STORE) % store);
        return false;
    }

    // Open store.
    console(BN_STORE_STARTING);
    if (const auto ec = store_.open([&](auto, auto)
    {
        ////console(format(BN_OPEN) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_START_FAIL) % ec.message());
        return false;
    }

    write_test();

    // Close store.
    console(BN_STORE_STOPPING);
    if (const auto ec = store_.close([&](auto, auto)
    {
        ////console(format(BN_CLOSE) % events_.at(event) % tables_.at(table));
    }))
    {
        console(format(BN_STORE_STOP_FAIL) % ec.message());
        return false;
    }

    console(BN_STORE_STOPPED);
    return true;
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
        uint8_t event, uint64_t value, const logger::time& point)
    {
        if (ec) return false;

        switch (event)
        {
            case event_archive:
            {
                const auto time = duration_cast<seconds>(point - start).count();
                sink << "[archive] " << value << " " << time << std::endl;
                break;
            }
            case event_header:
            {
                const auto time = duration_cast<seconds>(point - start).count();
                sink << "[header] " << value << " " << time << std::endl;
                break;
            }
            case event_block:
            {
                const auto time = duration_cast<seconds>(point - start).count();
                sink << "[block] " << value << " " << time << std::endl;
                break;
            }
            case event_current_headers:
            {
                const auto time = duration_cast<seconds>(point - start).count();
                sink << "[headers] " << value << " " << time << std::endl;
                break;
            }
            case event_current_blocks:
            {
                const auto time = duration_cast<seconds>(point - start).count();
                sink << "[blocks] " << value << " " << time << std::endl;
                break;
            }
            case event_validated:
            case event_confirmed:
            case event_current_validated:
            case event_current_confirmed:
            default: break;
        }

        return true;
    });
}

void executor::subscribe_capture()
{
    cap_.subscribe([&](const code& ec, const std::string& line)
    {
        const auto token = system::trim_copy(line);
        if (!keys_.contains(token))
        {
            logger("CONSOLE: '" + line + "'");
            return !ec;
        }

        const auto index = keys_.at(token);

        // Quit (this level isn't a toggle).
        if (index == levels::quit)
        {
            logger("CONSOLE: " + display_.at(index));
            stop(error::success);
            return false;
        }

        if (defined_.at(index))
        {
            toggle_.at(index) = !toggle_.at(index);
            logger("CONSOLE: " + display_.at(index) + (toggle_.at(index) ?
                " logging (+)." : " logging (-)."));
        }
        else
        {
            // Selected log level was not compiled.
            logger("CONSOLE: " + display_.at(index) + " logging (~).");
        }

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
        if (to_bool(metadata_.configured.node.interval) &&
            is_zero(node_->channel_count() %
                metadata_.configured.node.interval))
        {
            log_.write(levels::application) <<
                "{in:" << node_->inbound_channel_count() << "}"
                "{ch:" << node_->channel_count() << "}"
                "{rv:" << node_->reserved_count() << "}"
                "{nc:" << node_->nonces_count() << "}"
                "{ad:" << node_->address_count() << "}"
                "{ss:" << node_->stop_subscriber_count() << "}"
                "{cs:" << node_->connect_subscriber_count() << "}."
                << std::endl;
        }

        if (to_bool(metadata_.configured.node.target) &&
            (node_->channel_count() >= metadata_.configured.node.target))
        {
            log_.write(levels::application) << "Stopping at channel target ("
                << metadata_.configured.node.target << ")." << std::endl;

            // Signal stop (simulates <ctrl-c>).
            stop(error::success);
            return false;
        }

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
        log_.write(levels::application) <<
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
        console(BN_LOG_INITIALIZE_FAILURE);
        return false;
    }

    subscribe_log(log);
    subscribe_events(events);
    subscribe_capture();
    logger(BN_LOG_HEADER);

    const auto& file = metadata_.configured.file;
    if (file.empty())
        logger(BN_USING_DEFAULT_CONFIG);
    else
        logger(format(BN_USING_CONFIG_FILE) % file);

    // Verify store exists.
    const auto& store = metadata_.configured.database.path;
    if (!database::file::is_directory(store))
    {
        logger(format(BN_UNINITIALIZED_STORE) % store);
        stopper(BN_NODE_STOPPED);
        return false;
    }

    logger(BN_NODE_INTERRUPT);
    cap_.start();

    // Open store.
    logger(BN_STORE_STARTING);
    if (const auto ec = store_.open([&](auto event, auto table)
    {
        logger(format(BN_OPEN) % events_.at(event) % tables_.at(table));
    }))
    {
        logger(format(BN_STORE_START_FAIL) % ec.message());
        stopper(BN_NODE_STOPPED);
        return false;
    }

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
        query_.strong_tx_size() %
        query_.validated_tx_size() %
        query_.validated_bk_size());
    logger(format(BN_MEASURE_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.puts_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.strong_tx_records());
    logger(format(BN_MEASURE_BUCKETS) %
        query_.header_buckets() %
        query_.txs_buckets() %
        query_.tx_buckets() %
        query_.point_buckets() %
        query_.input_buckets() %
        query_.strong_tx_buckets() %
        query_.validated_tx_buckets() %
        query_.validated_bk_buckets());

    // Create node.
    metadata_.configured.network.initialize();
    node_ = std::make_shared<full_node>(query_, metadata_.configured, log_);

    // Subscribe node.
    subscribe_connect();
    subscribe_close();

    logger(format(BN_CHANNEL_LOG_PERIOD) % metadata_.configured.node.interval);
    logger(format(BN_CHANNEL_STOP_TARGET) % metadata_.configured.node.target);

    // Start network.
    logger(BN_NETWORK_STARTING);
    node_->start(std::bind(&executor::handle_started, this, _1));

    // Wait on signal to stop node (<ctrl-c>).
    stopping_.get_future().wait();
    logger(BN_NETWORK_STOPPING);

    // Stop network (if not already stopped by self).
    node_->close();

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
        query_.strong_tx_size() %
        query_.validated_tx_size() %
        query_.validated_bk_size());
    logger(format(BN_MEASURE_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.puts_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.strong_tx_records());

    // Close store (flush to disk).
    logger(BN_STORE_STOPPING);
    if (const auto ec = store_.close([&](auto event, auto table)
    {
        logger(format(BN_CLOSE) % events_.at(event) % tables_.at(table));
    }))
    {
        logger(format(BN_STORE_STOP_FAIL) % ec.message());
        stopper(BN_NODE_STOPPED);
        return false;
    }

    // Node is stopped.
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
