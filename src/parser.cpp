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
    // node

    configured.node.snapshot_bytes = 0;
    configured.node.snapshot_valid = 0;
    configured.node.snapshot_confirm = 0;

    // network

    using level = network::messages::level;
    using service = network::messages::service;

    configured.network.threads = 16;
    configured.network.enable_address = true;
    configured.network.enable_transaction = true;
    configured.network.host_pool_capacity = 10000;
    configured.network.outbound_connections = 100;
    configured.network.protocol_minimum = level::headers_protocol;
    configured.network.protocol_maximum = level::bip130;

    // services_minimum must be node_witness to be a witness node.
    configured.network.services_minimum = service::node_network |
        service::node_witness;
    configured.network.services_maximum = service::node_network |
        service::node_witness;

    // database

    configured.database.minimize = false;

    // archive

    configured.database.header_buckets = 524'493;
    configured.database.header_size = 20'397'669;
    configured.database.header_rate = 5;

    configured.database.txs_buckets = 524'493;
    configured.database.txs_size = 999'581'257;
    configured.database.txs_rate = 5;

    configured.database.tx_buckets = 551'320'125;
    configured.database.tx_size = 15'435'744'998;
    configured.database.tx_rate = 5;

    configured.database.point_buckets = 546'188'501;
    configured.database.point_size = 8'389'074'978;
    configured.database.point_rate = 5;

    configured.database.spend_buckets = 1'459'791'875;
    configured.database.spend_size = 15'364'630'530;
    configured.database.spend_rate = 5;

    configured.database.puts_size = 6'059'682'874;
    configured.database.puts_rate = 5;

    configured.database.input_size = 90'116'786'234;
    configured.database.input_rate = 5;

    configured.database.output_size = 24'315'563'831;
    configured.database.output_rate = 5;

    // metadata

    configured.database.candidate_size = 2'523'423;
    configured.database.candidate_rate = 5;

    configured.database.confirmed_size = 2'523'423;
    configured.database.confirmed_rate = 5;

    configured.database.strong_tx_buckets = 551'320'125;
    configured.database.strong_tx_size = 2'987'563'554;
    configured.database.strong_tx_rate = 5;

    configured.database.validated_tx_buckets = 551'320'125;
    configured.database.validated_tx_size = 47'068'879;
    configured.database.validated_tx_rate = 5;

    configured.database.validated_bk_buckets = 524'493;
    configured.database.validated_bk_size = 8'290;
    configured.database.validated_bk_rate = 5;

    // optional

    configured.database.address_buckets = 0;
    configured.database.address_size = 1;
    configured.database.address_rate = 5;

    configured.database.neutrino_buckets = 0;
    configured.database.neutrino_size =  1;
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
        BN_HARDWARE_VARIABLE ",d",
        value<bool>(&configured.hardware)->
            default_value(false)->zero_tokens(),
        "Display hardware compatibility."
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
        BN_NEWSTORE_VARIABLE ",n",
        value<bool>(&configured.newstore)->
            default_value(false)->zero_tokens(),
        "Create new store in configured directory."
    )
    (
        BN_BACKUP_VARIABLE ",b",
        value<bool>(&configured.backup)->
            default_value(false)->zero_tokens(),
        "Backup to a snapshot (can also do live)."
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
        "Scan and display all flag transitions."
    )
    (
        BN_SLABS_VARIABLE ",a",
        value<bool>(&configured.slabs)->
            default_value(false)->zero_tokens(),
        "Scan and display store slab measures."
    )
    (
        BN_BUCKETS_VARIABLE ",k",
        value<bool>(&configured.buckets)->
            default_value(false)->zero_tokens(),
        "Scan and display all bucket densities."
    )
    (
        BN_COLLISIONS_VARIABLE ",l",
        value<bool>(&configured.collisions)->
            default_value(false)->zero_tokens(),
        "Scan and display hashmap collision stats (may exceed RAM and result in SIGKILL)."
    )
    (
        BN_INFORMATION_VARIABLE ",i",
        value<bool>(&configured.information)->
            default_value(false)->zero_tokens(),
        "Scan and display store information."
    )
    // Ad-hoc Testing.
    (
        BN_READ_VARIABLE ",t",
        value<bool>(&configured.test)->
            default_value(false)->zero_tokens(),
        "Run built-in read test and display."
    )
    (
        BN_WRITE_VARIABLE ",w",
        value<bool>(&configured.write)->
            default_value(false)->zero_tokens(),
        "Run built-in write test and display."
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
        "A block presumed to be valid but not required to be present, defaults to 00000000000000000002a0b5db2a7f8d9087464c2586b546be7bce8eb53b8187:850000."
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
        "The minimum services exposed by network connections, defaults to 9 (full node, witness)."
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
        "The peer download rate limit in bytes per second, defaults to 1024 (not implemented)."
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
    (
        "database.minimize",
        value<bool>(&configured.database.minimize),
        "Minimize store, saves ~50GiB, requires high RAM to avoid thrashing, defaults to false."
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
        "The minimum allocation of the header table body, defaults to '20397669'."
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
        "The minimum allocation of the txs table body, defaults to '999581257'."
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
        "The minimum allocation of the tx table body, defaults to '15435744998'."
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
        "The minimum allocation of the point table body, defaults to '8389074978'."
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
        "The minimum allocation of the spend table body, defaults to '15364630530'."
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
        "The minimum allocation of the puts table body, defaults to '6059682874'."
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
        "The minimum allocation of the input table body, defaults to '90116786234'."
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
        "The minimum allocation of the output table body, defaults to '24315563831'."
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
        "The minimum allocation of the candidate table body, defaults to '2523423'."
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
        "The minimum allocation of the candidate table body, defaults to '2523423'."
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
        "The minimum allocation of the strong_tx table body, defaults to '2987563554'."
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
        "The minimum allocation of the validated_tx table body, defaults to '47068879'."
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
        "The minimum allocation of the validated_bk table body, defaults to '8290'."
    )
    (
        "database.validated_bk_rate",
        value<uint16_t>(&configured.database.validated_bk_rate),
        "The percentage expansion of the validated_bk table body, defaults to '5'."
    )

    /* address */
    (
        "database.address_buckets",
        value<uint32_t>(&configured.database.address_buckets),
        "The number of buckets in the address table head, defaults to '0' (0 disables)."
    )
    (
        "database.address_size",
        value<uint64_t>(&configured.database.address_size),
        "The minimum allocation of the address table body, defaults to '1'."
    )
    (
        "database.address_rate",
        value<uint16_t>(&configured.database.address_rate),
        "The percentage expansion of the address table body, defaults to '0'."
    )

    /* neutrino */
    (
        "database.neutrino_buckets",
        value<uint32_t>(&configured.database.neutrino_buckets),
        "The number of buckets in the neutrino table head, defaults to '0' (0 disables)."
    )
    (
        "database.neutrino_size",
        value<uint64_t>(&configured.database.neutrino_size),
        "The minimum allocation of the neutrino table body, defaults to '1'."
    )
    (
        "database.neutrino_rate",
        value<uint16_t>(&configured.database.neutrino_rate),
        "The percentage expansion of the neutrino table body, defaults to '0'."
    )

    /* [node] */
    (
        "node.threads",
        value<uint32_t>(&configured.node.threads),
        "The number of threads in the validation threadpool, defaults to 16."
    )
    (
        "node.prepopulate",
        value<bool>(&configured.node.prepopulate),
        "Populate block prevous from self before query [testing], defaults to true."
    )
    (
        "node.priority_validation",
        value<bool>(&configured.node.priority_validation),
        "Set the validation threadpool to high priority, defaults to false."
    )
    (
        "node.concurrent_validation",
        value<bool>(&configured.node.concurrent_validation),
        "Perform validation concurrently with download, defaults to false."
    )
    (
        "node.concurrent_confirmation",
        value<bool>(&configured.node.concurrent_confirmation),
        "Perform confirmation concurrently with download, defaults to false."
    )
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
        "node.allocation_multiple",
        value<uint16_t>(&configured.node.allocation_multiple),
        "Block deserialization buffer multiple of wire size, defaults to 20 (0 disables)."
    )
    (
        "node.maximum_height",
        value<uint32_t>(&configured.node.maximum_height),
        "Maximum block height to populate, defaults to 0 (unlimited)."
    )
    (
        "node.maximum_concurrency",
        value<uint32_t>(&configured.node.maximum_concurrency),
        "Maximum number of blocks to download concurrently, defaults to '50000' (0 disables)."
    )
    (
        "node.snapshot_bytes",
        value<uint64_t>(&configured.node.snapshot_bytes),
        "Downloaded bytes that triggers snapshot, defaults to '0' (0 disables)."
    )
    (
        "node.snapshot_valid",
        value<uint32_t>(&configured.node.snapshot_valid),
        "Completed validations that trigger snapshot, defaults to '0' (0 disables)."
    )
    (
        "node.snapshot_confirm",
        value<uint32_t>(&configured.node.snapshot_confirm),
        "Completed confirmations that trigger snapshot, defaults to '0' (0 disables)."
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
    // #######################
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
#if defined(HAVE_LOGA)
    (
        "log.application",
        value<bool>(&configured.log.application),
        "Enable application logging, defaults to true."
    )
#endif
#if defined(HAVE_LOGN)
    (
        "log.news",
        value<bool>(&configured.log.news),
        "Enable news logging, defaults to true."
    )
#endif
#if defined(HAVE_LOGS)
    (
        "log.session",
        value<bool>(&configured.log.session),
        "Enable session logging, defaults to true."
    )
#endif
#if defined(HAVE_LOGP)
    (
        "log.protocol",
        value<bool>(&configured.log.protocol),
        "Enable protocol logging, defaults to false."
    )
#endif
#if defined(HAVE_LOGX)
    (
        "log.proxy",
        value<bool>(&configured.log.proxy),
        "Enable proxy logging, defaults to false."
    )
#endif
#if defined(HAVE_LOGR)
    (
        "log.remote",
        value<bool>(&configured.log.remote),
        "Enable remote fault logging, defaults to true."
    )
#endif
#if defined(HAVE_LOGF)
    (
        "log.fault",
        value<bool>(&configured.log.fault),
        "Enable local fault logging, defaults to true."
    )
#endif
#if defined(HAVE_LOGQ)
    (
        "log.quitting",
        value<bool>(&configured.log.quitting),
        "Enable quitting logging, defaults to false."
    )
#endif
#if defined(HAVE_LOGO)
    (
        "log.objects",
        value<bool>(&configured.log.objects),
        "Enable objects logging, defaults to false."
    )
#endif
#if defined(HAVE_LOGV)
    (
        "log.verbose",
        value<bool>(&configured.log.verbose),
        "Enable verbose logging, defaults to false."
    )
#endif
    (
        "log.maximum_size",
        value<uint32_t>(&configured.log.maximum_size),
        "The maximum byte size of each pair of rotated log files, defaults to 1000000."
    )
#if defined (HAVE_MSC)
    (
        "log.symbols",
        value<std::filesystem::path>(&configured.log.symbols),
        "Path to windows debug build symbols file (.pdb)."
    )
#endif
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
