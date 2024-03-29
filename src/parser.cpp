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
#include <bitcoin/node/parser.hpp>

#include <iostream>
#include <bitcoin/system.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>

std::filesystem::path config_default_path() NOEXCEPT
{
    return { "libbitcoin/bn.cfg" };
}

namespace libbitcoin {
namespace node {

using namespace bc::system;
using namespace bc::system::config;
using namespace boost::program_options;

// Initialize configuration by copying the given instance.
parser::parser(const configuration& defaults) NOEXCEPT
  : configured(defaults)
{
}

// Initialize configuration using defaults of the given context.
parser::parser(system::chain::selection context) NOEXCEPT
  : configured(context)
{
    using level = network::messages::level;
    using service = network::messages::service;

    configured.network.threads = 16;
    configured.network.enable_address = true;
    configured.network.enable_transaction = true;
    configured.network.host_pool_capacity = 10000;
    configured.network.outbound_connections = 100;
    configured.network.protocol_minimum = level::headers_protocol;
    configured.network.protocol_maximum = level::bip130;
    configured.network.services_minimum = service::node_network;
    configured.network.services_maximum = service::node_network | 
        service::node_witness;

    // archive

    configured.database.header_buckets = 524'493;
    configured.database.header_size = 77'141'872;
    configured.database.header_rate = 5;

    configured.database.txs_buckets = 524'493;
    configured.database.txs_size = 3'357'982'206;
    configured.database.txs_rate = 5;

    configured.database.tx_buckets = 551'320'125;
    configured.database.tx_size = 51'913'129'453;
    configured.database.tx_rate = 5;

    configured.database.point_buckets = 546'188'501;
    configured.database.point_size = 2'984'0089'620;
    configured.database.point_rate = 5;

    configured.database.spend_buckets = 1'459'791'875;
    configured.database.spend_size = 53'206'231'790;
    configured.database.spend_rate = 5;

    configured.database.puts_size = 20'659'465'287;
    configured.database.puts_rate = 5;

    configured.database.input_size = 305'136'375'030;
    configured.database.input_rate = 5;

    configured.database.output_size = 81'451'591'673;
    configured.database.output_rate = 5;

    // metadata

    configured.database.candidate_size = 2'385'831;
    configured.database.candidate_rate = 5;

    configured.database.confirmed_size = 2'385'831;
    configured.database.confirmed_rate = 5;

    configured.database.strong_tx_buckets = 551'320'125;
    configured.database.strong_tx_size = 9'187'749'878;
    configured.database.strong_tx_rate = 5;

    configured.database.validated_tx_buckets = 551'320'125;
    configured.database.validated_tx_size = 18'992'429'906;
    configured.database.validated_tx_rate = 5;

    configured.database.validated_bk_buckets = 524'493;
    configured.database.validated_bk_size = 6'322'979;
    configured.database.validated_bk_rate = 5;

    // optional

    configured.database.bootstrap_size = 1;
    configured.database.bootstrap_rate = 5;

    configured.database.address_buckets = 100;
    configured.database.address_size = 1;
    configured.database.address_rate = 5;

    configured.database.buffer_buckets = 100;
    configured.database.buffer_size = 1;
    configured.database.buffer_rate = 5;

    configured.database.neutrino_buckets = 100;
    configured.database.neutrino_size = 1;
    configured.database.neutrino_rate = 5;
}

options_metadata parser::load_options() THROWS
{
    options_metadata description("options");
    description.add_options()
    (
        BN_CONFIG_VARIABLE ",c",
        value<std::filesystem::path>(&configured.file),
        "Specify path to a configuration settings file."
    )
    // Information.
    (
        BN_HELP_VARIABLE ",h",
        value<bool>(&configured.help)->
            default_value(false)->zero_tokens(),
        "Display command line options."
    )
    (
        BN_SETTINGS_VARIABLE ",s",
        value<bool>(&configured.settings)->
            default_value(false)->zero_tokens(),
        "Display all configuration settings."
    )
    (
        BN_VERSION_VARIABLE ",v",
        value<bool>(&configured.version)->
            default_value(false)->zero_tokens(),
        "Display version information."
    )
    // Actions.
    (
        BN_INITCHAIN_VARIABLE ",i",
        value<bool>(&configured.initchain)->
            default_value(false)->zero_tokens(),
        "Initialize store in configured directory."
    )
    (
        BN_RESTORE_VARIABLE ",r",
        value<bool>(&configured.restore)->
            default_value(false)->zero_tokens(),
        "Restore from most recent snapshot."
    )
    // Chain scans.
    (
        BN_FLAGS_VARIABLE ",f",
        value<bool>(&configured.flags)->
            default_value(false)->zero_tokens(),
        "Compute and display all flag transitions."
    )
    (
        BN_MEASURE_VARIABLE ",m",
        value<bool>(&configured.measure)->
            default_value(false)->zero_tokens(),
        "Compute and display store measures."
    )
    (
        BN_BUCKETS_VARIABLE ",b",
        value<bool>(&configured.buckets)->
            default_value(false)->zero_tokens(),
        "Compute and display all bucket densities."
    )
    (
        BN_COLLISIONS_VARIABLE ",l",
        value<bool>(&configured.collisions)->
            default_value(false)->zero_tokens(),
        "Compute and display hashmap collision stats."
    )
    // Ad-hoc Testing.
    (
        BN_READ_VARIABLE ",x",
        value<bool>(&configured.read)->
            default_value(false)->zero_tokens(),
        "Read test and display performance."
    )
    (
        BN_WRITE_VARIABLE ",w",
        value<bool>(&configured.write)->
            default_value(false)->zero_tokens(),
        "Write test and display performance."
    );

    return description;
}

arguments_metadata parser::load_arguments() THROWS
{
    arguments_metadata description;
    return description
        .add(BN_CONFIG_VARIABLE, 1);
}

options_metadata parser::load_environment() THROWS
{
    options_metadata description("environment");
    description.add_options()
    (
        // For some reason po requires this to be a lower case name.
        // The case must match the other declarations for it to compose.
        // This composes with the cmdline options and inits to default path.
        BN_CONFIG_VARIABLE,
        value<std::filesystem::path>(&configured.file)->composing()
            ->default_value(config_default_path()),
        "The path to the configuration settings file."
    );

    return description;
}

options_metadata parser::load_settings() THROWS
{
    options_metadata description("settings");
    description.add_options()

    /* [forks] */
    (
        "forks.difficult",
        value<bool>(&configured.bitcoin.forks.difficult),
        "Require difficult blocks, defaults to true (use false for testnet)."
    )
    (
        "forks.retarget",
        value<bool>(&configured.bitcoin.forks.retarget),
        "Retarget difficulty, defaults to true."
    )
    (
        "forks.bip16",
        value<bool>(&configured.bitcoin.forks.bip16),
        "Add pay-to-script-hash processing, defaults to true (soft fork)."
    )
    (
        "forks.bip30",
        value<bool>(&configured.bitcoin.forks.bip30),
        "Disallow collision of unspent transaction hashes, defaults to true (soft fork)."
    )
    (
        "forks.bip34",
        value<bool>(&configured.bitcoin.forks.bip34),
        "Require coinbase input includes block height, defaults to true (soft fork)."
    )
    (
        "forks.bip42",
        value<bool>(&configured.bitcoin.forks.bip42),
        "Finite monetary supply, defaults to true (soft fork)."
    )
    (
        "forks.bip66",
        value<bool>(&configured.bitcoin.forks.bip66),
        "Require strict signature encoding, defaults to true (soft fork)."
    )
    (
        "forks.bip65",
        value<bool>(&configured.bitcoin.forks.bip65),
        "Add check-locktime-verify op code, defaults to true (soft fork)."
    )
    (
        "forks.bip90",
        value<bool>(&configured.bitcoin.forks.bip90),
        "Assume bip34, bip65, and bip66 activation if enabled, defaults to true (hard fork)."
    )
    (
        "forks.bip68",
        value<bool>(&configured.bitcoin.forks.bip68),
        "Add relative locktime enforcement, defaults to true (soft fork)."
    )
    (
        "forks.bip112",
        value<bool>(&configured.bitcoin.forks.bip112),
        "Add check-sequence-verify op code, defaults to true (soft fork)."
    )
    (
        "forks.bip113",
        value<bool>(&configured.bitcoin.forks.bip113),
        "Use median time past for locktime, defaults to true (soft fork)."
    )
    (
        "forks.bip141",
        value<bool>(&configured.bitcoin.forks.bip141),
        "Segregated witness consensus layer, defaults to true (soft fork)."
    )
    (
        "forks.bip143",
        value<bool>(&configured.bitcoin.forks.bip143),
        "Version 0 transaction digest, defaults to true (soft fork)."
    )
    (
        "forks.bip147",
        value<bool>(&configured.bitcoin.forks.bip147),
        "Prevent dummy value malleability, defaults to true (soft fork)."
    )
    (
        "forks.time_warp_patch",
        value<bool>(&configured.bitcoin.forks.time_warp_patch),
        "Fix time warp bug, defaults to false (hard fork)."
    )
    (
        "forks.retarget_overflow_patch",
        value<bool>(&configured.bitcoin.forks.retarget_overflow_patch),
        "Fix target overflow for very low difficulty, defaults to false (hard fork)."
    )
    (
        "forks.scrypt_proof_of_work",
        value<bool>(&configured.bitcoin.forks.scrypt_proof_of_work),
        "Use scrypt hashing for proof of work, defaults to false (hard fork)."
    )

    /* [bitcoin] */
    (
        "bitcoin.initial_block_subsidy_bitcoin",
        value<uint64_t>(&configured.bitcoin.initial_subsidy_bitcoin),
        "The initial block subsidy, defaults to 50."
    )
    (
        "bitcoin.subsidy_interval",
        value<uint32_t>(&configured.bitcoin.subsidy_interval_blocks),
        "The subsidy halving period, defaults to 210000."
    )
    (
        "bitcoin.timestamp_limit_seconds",
        value<uint32_t>(&configured.bitcoin.timestamp_limit_seconds),
        "The future timestamp allowance, defaults to 7200."
    )
    (
        "bitcoin.retargeting_factor",
        value<uint32_t>(&configured.bitcoin.retargeting_factor),
        "The difficulty retargeting factor, defaults to 4."
    )
    (
        "bitcoin.retargeting_interval_seconds",
        value<uint32_t>(&configured.bitcoin.retargeting_interval_seconds),
        "The difficulty retargeting period, defaults to 1209600."
    )
    (
        "bitcoin.block_spacing_seconds",
        value<uint32_t>(&configured.bitcoin.block_spacing_seconds),
        "The target block period, defaults to 600."
    )
    (
        "bitcoin.proof_of_work_limit",
        value<uint32_t>(&configured.bitcoin.proof_of_work_limit),
        "The proof of work limit, defaults to 486604799."
    )
    (
        "bitcoin.genesis_block",
        value<config::block>(&configured.bitcoin.genesis_block),
        "The genesis block, defaults to mainnet."
    )
    (
        "bitcoin.checkpoint",
        value<chain::checkpoints>(&configured.bitcoin.checkpoints),
        "The blockchain checkpoints, defaults to the consensus set."
    )
    // [version properties excluded here]
    (
        "bitcoin.bip34_activation_threshold",
        value<size_t>(&configured.bitcoin.bip34_activation_threshold),
        "The number of new version blocks required for bip34 style soft fork activation, defaults to 750."
    )
    (
        "bitcoin.bip34_enforcement_threshold",
        value<size_t>(&configured.bitcoin.bip34_enforcement_threshold),
        "The number of new version blocks required for bip34 style soft fork enforcement, defaults to 950."
    )
    (
        "bitcoin.bip34_activation_sample",
        value<size_t>(&configured.bitcoin.bip34_activation_sample),
        "The number of blocks considered for bip34 style soft fork activation, defaults to 1000."
    )
    (
        "bitcoin.bip65_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip65_height),
        "The block height to freeze the bip65 softfork for bip90, defaults to 388381."
    )
    (
        "bitcoin.bip66_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip66_height),
        "The block height to freeze the bip66 softfork for bip90, defaults to 363725."
    )
    (
        "bitcoin.bip34_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip34_height),
        "The block height to freeze the bip34 softfork for bip90, defaults to 227931."
    )
    (
        "bitcoin.bip16_activation_time",
        value<uint32_t>(&configured.bitcoin.bip16_activation_time),
        "The activation time for bip16 in unix time, defaults to 1333238400."
    )
    (
        "bitcoin.bip9_bit0_active_checkpoint",
        value<chain::checkpoint>(&configured.bitcoin.bip9_bit0_active_checkpoint),
        "The hash:height checkpoint for bip9 bit0 activation, defaults to 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5:419328."
    )
    (
        "bitcoin.milestone",
        value<chain::checkpoint>(&configured.bitcoin.milestone),
        "A block presumed to be valid but not required to be present, defaults to 00000000000000000001a0a448d6cf2546b06801389cc030b2b18c6491266815:804000."
    )
    (
        "bitcoin.minimum_work",
        value<config::hash256>(&configured.bitcoin.minimum_work),
        "The minimum work for any branch to be considered valid, defaults to 000000000000000000000000000000000000000052b2559353df4117b7348b64."
    )

    /* [network] */
    (
        "network.threads",
        value<uint32_t>(&configured.network.threads),
        "The minimum number of threads in the network threadpool, defaults to 16."
    )
    (
        "network.address_upper",
        value<uint16_t>(&configured.network.address_upper),
        "The upper bound for address selection divisor, defaults to 10."
    )
    (
        "network.address_lower",
        value<uint16_t>(&configured.network.address_lower),
        "The lower bound for address selection divisor, defaults to 5."
    )
    (
        "network.protocol_maximum",
        value<uint32_t>(&configured.network.protocol_maximum),
        "The maximum network protocol version, defaults to 70012."
    )
    (
        "network.protocol_minimum",
        value<uint32_t>(&configured.network.protocol_minimum),
        "The minimum network protocol version, defaults to 31800."
    )
    (
        "network.services_maximum",
        value<uint64_t>(&configured.network.services_maximum),
        "The maximum services exposed by network connections, defaults to 9 (full node, witness)."
    )
    (
        "network.services_minimum",
        value<uint64_t>(&configured.network.services_minimum),
        "The minimum services exposed by network connections, defaults to 1 (full node)."
    )
    (
        "network.invalid_services",
        value<uint64_t>(&configured.network.invalid_services),
        "The advertised services that cause a peer to be dropped, defaults to 176."
    )
    (
        "network.enable_address",
        value<bool>(&configured.network.enable_address),
        "Enable address messages, defaults to true."
    )
    (
        "network.enable_alert",
        value<bool>(&configured.network.enable_alert),
        "Enable alert messages, defaults to false."
    )
    (
        "network.enable_reject",
        value<bool>(&configured.network.enable_reject),
        "Enable reject messages, defaults to false."
    )
    (
        "network.enable_transaction",
        value<bool>(&configured.network.enable_transaction),
        "Enable transaction relay, defaults to true."
    )
    (
        "network.enable_ipv6",
        value<bool>(&configured.network.enable_ipv6),
        "Enable internet protocol version 6 (IPv6), defaults to false."
    )
    (
        "network.enable_loopback",
        value<bool>(&configured.network.enable_loopback),
        "Allow connections from the node to itself, defaults to false."
    )
    (
        "network.validate_checksum",
        value<bool>(&configured.network.validate_checksum),
        "Validate the checksum of network messages, defaults to false."
    )
    (
        "network.identifier",
        value<uint32_t>(&configured.network.identifier),
        "The magic number for message headers, defaults to 3652501241."
    )
    (
        "network.inbound_connections",
        value<uint16_t>(&configured.network.inbound_connections),
        "The target number of incoming network connections, defaults to 0."
    )
    (
        "network.outbound_connections",
        value<uint16_t>(&configured.network.outbound_connections),
        "The target number of outgoing network connections, defaults to 100."
    )
    (
        "network.connect_batch_size",
        value<uint16_t>(&configured.network.connect_batch_size),
        "The number of concurrent attempts to establish one connection, defaults to 5."
    )
    (
        "network.retry_timeout_seconds",
        value<uint32_t>(&configured.network.retry_timeout_seconds),
        "The time delay for failed connection retry, defaults to 1."
    )
    (
        "network.connect_timeout_seconds",
        value<uint32_t>(&configured.network.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to 5."
    )
    (
        "network.handshake_timeout_seconds",
        value<uint32_t>(&configured.network.handshake_timeout_seconds),
        "The time limit to complete the connection handshake, defaults to 30."
    )
    (
        "network.seeding_timeout_seconds",
        value<uint32_t>(&configured.network.seeding_timeout_seconds),
        "The time limit for obtaining seed addresses, defaults to 30."
    )
    (
        "network.channel_heartbeat_minutes",
        value<uint32_t>(&configured.network.channel_heartbeat_minutes),
        "The time between ping messages, defaults to 5."
    )
    (
        "network.channel_inactivity_minutes",
        value<uint32_t>(&configured.network.channel_inactivity_minutes),
        "The inactivity time limit for any connection, defaults to 10."
    )
    (
        "network.channel_expiration_minutes",
        value<uint32_t>(&configured.network.channel_expiration_minutes),
        "The age limit for any connection, defaults to 1440."
    )
    (
        "network.host_pool_capacity",
        value<uint32_t>(&configured.network.host_pool_capacity),
        "The maximum number of peer hosts in the pool, defaults to 10000."
    )
    (
        "network.minimum_buffer",
        value<uint32_t>(&configured.network.minimum_buffer),
        "The minimum retained read buffer size, defaults to 4000000."
    )
    (
        "network.rate_limit",
        value<uint32_t>(&configured.network.rate_limit),
        "The peer download rate limit in bytes per second, defaults to 1024."
    )
    (
        "network.user_agent",
        value<std::string>(&configured.network.user_agent),
        "The node user agent string, defaults to '" BC_USER_AGENT "'."
    )
    (
        "network.path",
        value<std::filesystem::path>(&configured.network.path),
        "The peer address cache file directory, defaults to empty."
    )
    (
        "network.bind",
        value<network::config::authorities>(&configured.network.binds),
        "IP address to bind, multiple entries allowed, defaults to 0.0.0.0:8333."
    )
    (
        "network.self",
        value<network::config::authorities>(&configured.network.selfs),
        "IP address to advertise, multiple entries allowed."
    )
    (
        "network.blacklist",
        value<network::config::authorities>(&configured.network.blacklists),
        "IP address to disallow as a peer, multiple entries allowed."
    )
    (
        "network.whitelist",
        value<network::config::authorities>(&configured.network.whitelists),
        "IP address to allow as a peer, multiple entries allowed."
    )
    (
        "network.peer",
        value<network::config::endpoints>(&configured.network.peers),
        "A persistent peer node, multiple entries allowed."
    )
    (
        "network.seed",
        value<network::config::endpoints>(&configured.network.seeds),
        "A seed node for initializing the host pool, multiple entries allowed."
    )

    /* [database] */
    (
        "database.path",
        value<std::filesystem::path>(&configured.database.path),
        "The blockchain database directory, defaults to 'blockchain'."
    )

    /* header */
    (
        "database.header_buckets",
        value<uint32_t>(&configured.database.header_buckets),
        "The number of buckets in the header table head, defaults to '524493'."
    )
    (
        "database.header_size",
        value<uint64_t>(&configured.database.header_size),
        "The minimum allocation of the header table body, defaults to '77076918'."
    )
    (
        "database.header_rate",
        value<uint16_t>(&configured.database.header_rate),
        "The percentage expansion of the header table body, defaults to '5'."
    )

    /* txs */
    (
        "database.txs_buckets",
        value<uint32_t>(&configured.database.txs_buckets),
        "The number of buckets in the txs table head, defaults to '524493'."
    )
    (
        "database.txs_size",
        value<uint64_t>(&configured.database.txs_size),
        "The minimum allocation of the txs table body, defaults to '3349740645'."
    )
    (
        "database.txs_rate",
        value<uint16_t>(&configured.database.txs_rate),
        "The percentage expansion of the txs table body, defaults to '5'."
    )

    /* tx */
    (
        "database.tx_buckets",
        value<uint32_t>(&configured.database.tx_buckets),
        "The number of buckets in the tx table head, defaults to '551320125'."
    )
    (
        "database.tx_size",
        value<uint64_t>(&configured.database.tx_size),
        "The minimum allocation of the tx table body, defaults to '51785499310'."
    )
    (
        "database.tx_rate",
        value<uint16_t>(&configured.database.tx_rate),
        "The percentage expansion of the tx table body, defaults to '5'."
    )

    /* point */
    (
        "database.point_buckets",
        value<uint32_t>(&configured.database.point_buckets),
        "The number of buckets in the point table head, defaults to '546188501'."
    )
    (
        "database.point_size",
        value<uint64_t>(&configured.database.point_size),
        "The minimum allocation of the point table body, defaults to '29789120826'."
    )
    (
        "database.point_rate",
        value<uint16_t>(&configured.database.point_rate),
        "The percentage expansion of the point table body, defaults to '5'."
    )

    /* spend */
    (
        "database.spend_buckets",
        value<uint32_t>(&configured.database.spend_buckets),
        "The number of buckets in the spend table head, defaults to '1459791875'."
    )
    (
        "database.spend_size",
        value<uint64_t>(&configured.database.spend_size),
        "The minimum allocation of the spend table body, defaults to '53097103092'."
    )
    (
        "database.spend_rate",
        value<uint16_t>(&configured.database.spend_rate),
        "The percentage expansion of the spend table body, defaults to '5'."
    )

    /* puts */
    (
        "database.puts_size",
        value<uint64_t>(&configured.database.puts_size),
        "The minimum allocation of the puts table body, defaults to '20613115442'."
    )
    (
        "database.puts_rate",
        value<uint16_t>(&configured.database.puts_rate),
        "The percentage expansion of the puts table body, defaults to '5'."
    )

    /* input */
    (
        "database.input_size",
        value<uint64_t>(&configured.database.input_size),
        "The minimum allocation of the input table body, defaults to '300213426682'."
    )
    (
        "database.input_rate",
        value<uint16_t>(&configured.database.input_rate),
        "The percentage expansion of the input table body, defaults to '5'."
    )

    /* output */
    (
        "database.output_size",
        value<uint64_t>(&configured.database.output_size),
        "The minimum allocation of the output table body, defaults to '81253269896'."
    )
    (
        "database.output_rate",
        value<uint16_t>(&configured.database.output_rate),
        "The percentage expansion of the output table body, defaults to '5'."
    )

    /* candidate */
    (
        "database.candidate_size",
        value<uint64_t>(&configured.database.candidate_size),
        "The minimum allocation of the candidate table body, defaults to '2383822'."
    )
    (
        "database.candidate_rate",
        value<uint16_t>(&configured.database.candidate_rate),
        "The percentage expansion of the candidate table body, defaults to '5'."
    )

    /* confirmed */
    (
        "database.confirmed_size",
        value<uint64_t>(&configured.database.confirmed_size),
        "The minimum allocation of the candidate table body, defaults to '2383822'."
    )
    (
        "database.confirmed_rate",
        value<uint16_t>(&configured.database.confirmed_rate),
        "The percentage expansion of the candidate table body, defaults to '5'."
    )

    /* strong_tx */
    (
        "database.strong_tx_buckets",
        value<uint32_t>(&configured.database.strong_tx_buckets),
        "The number of buckets in the strong_tx table head, defaults to '551320125'."
    )
    (
        "database.strong_tx_size",
        value<uint64_t>(&configured.database.strong_tx_size),
        "The minimum allocation of the strong_tx table body, defaults to '9187749878'."
    )
    (
        "database.strong_tx_rate",
        value<uint16_t>(&configured.database.strong_tx_rate),
        "The percentage expansion of the strong_tx table body, defaults to '5'."
    )

    /* validated_tx */
    (
        "database.validated_tx_buckets",
        value<uint32_t>(&configured.database.validated_tx_buckets),
        "The number of buckets in the validated_tx table head, defaults to '551320125'."
    )
    (
        "database.validated_tx_size",
        value<uint64_t>(&configured.database.validated_tx_size),
        "The minimum allocation of the validated_tx table body, defaults to '18992429906'."
    )
    (
        "database.validated_tx_rate",
        value<uint16_t>(&configured.database.validated_tx_rate),
        "The percentage expansion of the validated_tx table body, defaults to '5'."
    )

    /* validated_bk */
    (
        "database.validated_bk_buckets",
        value<uint32_t>(&configured.database.validated_bk_buckets),
        "The number of buckets in the validated_bk table head, defaults to '524493'."
    )
    (
        "database.validated_bk_size",
        value<uint64_t>(&configured.database.validated_bk_size),
        "The minimum allocation of the validated_bk table body, defaults to '6322979'."
    )
    (
        "database.validated_bk_rate",
        value<uint16_t>(&configured.database.validated_bk_rate),
        "The percentage expansion of the validated_bk table body, defaults to '5'."
    )

    /* [node] */
    (
        "node.headers_first",
        value<bool>(&configured.node.headers_first),
        "Obtain current header chain before obtaining associated blocks, defaults to true."
    )
    (
        "node.allowed_deviation",
        value<float>(&configured.node.allowed_deviation),
        "Allowable underperformance standard deviation, defaults to 1.5 (0 disables)."
    )
    (
        "node.sample_period_seconds",
        value<uint16_t>(&configured.node.sample_period_seconds),
        "Sampling period for drop of stalled channels, defaults to 10 (0 disables)."
    )
    (
        "node.currency_window_minutes",
        value<uint32_t>(&configured.node.currency_window_minutes),
        "Time from present that blocks are considered current, defaults to 60 (0 disables)."
    )
    (
        "node.maximum_inventory",
        value<uint16_t>(&configured.node.maximum_inventory),
        "Maximum size of block inventory requests, defaults to 8000."
    )
    // ### temporary hacks ###
    (
        "node.target",
        value<uint16_t>(&configured.node.target),
        "Channel count that triggers node stop, defaults to 0 (0 disables)."
    )
    (
        "node.interval",
        value<uint16_t>(&configured.node.interval),
        "Channel count reporting interval, defaults to 0 (0 disables)."
    )
    ////(
    ////    "node.notify_limit_hours",
    ////    value<uint32_t>(&configured.node.notify_limit_hours),
    ////    "Disable relay when top block age exceeds, defaults to 24 (0 disables)."
    ////)
    ////(
    ////    "node.byte_fee_satoshis",
    ////    value<float>(&configured.node.byte_fee_satoshis),
    ////    "The minimum fee per byte, cumulative for conflicts, defaults to 1."
    ////)
    ////(
    ////    "node.sigop_fee_satoshis",
    ////    value<float>(&configured.node.sigop_fee_satoshis),
    ////    "The minimum fee per sigop, additional to byte fee, defaults to 100."
    ////)
    ////(
    ////    "node.minimum_output_satoshis",
    ////    value<uint64_t>(&configured.node.minimum_output_satoshis),
    ////    "The minimum output value, defaults to 500."
    ////)

    /* [log] */
#if defined (HAVE_MSC)
    (
        "log.symbols",
        value<std::filesystem::path>(&configured.log.symbols),
        "Path to windows debug build symbols file (.pdb)."
    )
#endif
    ////(
    ////    "log.verbose",
    ////    value<bool>(&configured.log.verbose),
    ////    "Enable verbose logging, defaults to false."
    ////)
    (
        "log.maximum_size",
        value<uint32_t>(&configured.log.maximum_size),
        "The maximum byte size of each pair of rotated log files, defaults to 1000000."
    )
    (
        "log.path",
        value<std::filesystem::path>(&configured.log.path),
        "The log files directory, defaults to empty."
    );

    return description;
}

bool parser::parse(int argc, const char* argv[], std::ostream& error) THROWS
{
    try
    {
        auto file = false;
        variables_map variables;
        load_command_variables(variables, argc, argv);
        load_environment_variables(variables, BN_ENVIRONMENT_VARIABLE_PREFIX);

        // Don't load config file if any of these options are specified.
        if (!get_option(variables, BN_VERSION_VARIABLE) &&
            !get_option(variables, BN_SETTINGS_VARIABLE) &&
            !get_option(variables, BN_HELP_VARIABLE))
        {
            // Returns true if the settings were loaded from a file.
            file = load_configuration_variables(variables, BN_CONFIG_VARIABLE);
        }

        // Update bound variables in metadata.settings.
        notify(variables);

        // Clear the config file path if it wasn't used.
        if (!file)
            configured.file.clear();
    }
    catch (const boost::program_options::error& e)
    {
        // This is obtained from boost, which circumvents our localization.
        error << format_invalid_parameter(e.what()) << std::endl;
        return false;
    }

    return true;
}

} // namespace node
} // namespace libbitcoin
