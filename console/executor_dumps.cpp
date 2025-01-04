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
#include <functional>
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

// Store dumps.
// ----------------------------------------------------------------------------

// emit version information for libbitcoin libraries
void executor::dump_version() const
{
    logger(format(BN_VERSION_MESSAGE)
        % LIBBITCOIN_NODE_VERSION
        % LIBBITCOIN_DATABASE_VERSION
        % LIBBITCOIN_NETWORK_VERSION
        % LIBBITCOIN_SYSTEM_VERSION);
}

// The "try" functions are safe for instructions not compiled in.
void executor::dump_hardware() const
{
    logger(BN_HARDWARE_HEADER);
    logger(format("arm..... " BN_HARDWARE_TABLE1) % with_arm);
    logger(format("intel... " BN_HARDWARE_TABLE1) % with_xcpu);
    logger(format("avx512.. " BN_HARDWARE_TABLE2) % system::try_avx512() % with_avx512);
    logger(format("avx2.... " BN_HARDWARE_TABLE2) % system::try_avx2() % with_avx2);
    logger(format("sse41... " BN_HARDWARE_TABLE2) % system::try_sse41() % with_sse41);
    logger(format("shani... " BN_HARDWARE_TABLE2) % system::try_shani() % with_shani);
    logger(format("neon.... " BN_HARDWARE_TABLE2) % system::try_neon() % with_neon);
}

// logging compilation and initial values.
void executor::dump_options() const
{
    logger(BN_LOG_TABLE_HEADER);
    logger(format("[a]pplication.. " BN_LOG_TABLE) % levels::application_defined % toggle_.at(levels::application));
    logger(format("[n]ews......... " BN_LOG_TABLE) % levels::news_defined % toggle_.at(levels::news));
    logger(format("[s]ession...... " BN_LOG_TABLE) % levels::session_defined % toggle_.at(levels::session));
    logger(format("[p]rotocol..... " BN_LOG_TABLE) % levels::protocol_defined % toggle_.at(levels::protocol));
    logger(format("[x]proxy....... " BN_LOG_TABLE) % levels::proxy_defined % toggle_.at(levels::proxy));
    logger(format("[r]emote....... " BN_LOG_TABLE) % levels::remote_defined % toggle_.at(levels::remote));
    logger(format("[f]ault........ " BN_LOG_TABLE) % levels::fault_defined % toggle_.at(levels::fault));
    logger(format("[q]uitting..... " BN_LOG_TABLE) % levels::quitting_defined % toggle_.at(levels::quitting));
    logger(format("[o]bjects...... " BN_LOG_TABLE) % levels::objects_defined % toggle_.at(levels::objects));
    logger(format("[v]erbose...... " BN_LOG_TABLE) % levels::verbose_defined % toggle_.at(levels::verbose));
}

void executor::dump_body_sizes() const
{
    logger(format(BN_MEASURE_SIZES) %
        query_.header_body_size() %
        query_.txs_body_size() %
        query_.tx_body_size() %
        query_.point_body_size() %
        query_.input_body_size() %
        query_.output_body_size() %
        query_.puts_body_size() %
        query_.candidate_body_size() %
        query_.confirmed_body_size() %
        query_.spend_body_size() %
        query_.strong_tx_body_size() %
        query_.validated_tx_body_size() %
        query_.validated_bk_body_size() %
        query_.address_body_size() %
        query_.neutrino_body_size());
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

// txs, validated_tx, validated_bk collision rates assume 1:1 records.
void executor::dump_collisions() const
{
    logger(format(BN_MEASURE_COLLISION_RATES) %
        (to_double(query_.header_records()) / query_.header_buckets()) %
        (to_double(query_.header_records()) / query_.txs_buckets()) %
        (to_double(query_.tx_records()) / query_.tx_buckets()) %
        (to_double(query_.point_records()) / query_.point_buckets()) %
        (to_double(query_.spend_records()) / query_.spend_buckets()) %
        (to_double(query_.strong_tx_records()) / query_.strong_tx_buckets()) %
        (to_double(query_.tx_records()) / query_.validated_tx_buckets()) %
        (to_double(query_.header_records()) / query_.validated_bk_buckets()) %
        (query_.address_enabled() ? (to_double(query_.address_records()) /
            query_.address_buckets()) : 0) %
        (query_.neutrino_enabled() ? (to_double(query_.header_records()) /
            query_.neutrino_buckets()) : 0));
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

// file and logical sizes.
void executor::dump_sizes() const
{
    dump_body_sizes();
    dump_records();
    dump_buckets();
    dump_collisions();

    // This one can take a few seconds on cold iron.
    logger(BN_MEASURE_PROGRESS_START);
    dump_progress();
}

} // namespace node
} // namespace libbitcoin
