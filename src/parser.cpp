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
#include <bitcoin/node/parser.hpp>

#include <iostream>
#include <bitcoin/system.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/settings.hpp>

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
    using service = network::messages::service;
    configured.chain.index_payments = false;
    configured.network.enable_relay = true;
    configured.network.host_pool_capacity = 10000;
    configured.network.outbound_connections = 10;
    configured.network.connect_batch_size = 5;
    configured.network.services_minimum = service::node_network;
    configured.network.services_maximum = service::node_network | service::node_witness;
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
    (
        BN_HELP_VARIABLE ",h",
        value<bool>(&configured.help)->
            default_value(false)->zero_tokens(),
        "Display command line options."
    )
    (
        BN_INITCHAIN_VARIABLE ",i",
        value<bool>(&configured.initchain)->
            default_value(false)->zero_tokens(),
        "Initialize blockchain in the configured directory."
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
    (
        BN_LIGHT_VARIABLE ",l",
        value<bool>(&configured.light)->
            default_value(false)->zero_tokens(),
        "Disable console logging."
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

    /* [log] */
    (
        "log.verbose",
        value<bool>(&configured.log.verbose),
        "Enable verbose logging, defaults to false."
    )
    (
        "log.maximum_size",
        value<uint32_t>(&configured.log.maximum_size),
        "The maximum byte size of each pair of rotated log files, defaults to 1000000."
    )
    (
        "log.path",
        value<std::filesystem::path>(&configured.log.path),
        "The log files directory, defaults to empty."
    )

    /* [bitcoin] */
    (
        "bitcoin.timestamp_limit_seconds",
        value<uint32_t>(&configured.bitcoin.timestamp_limit_seconds),
        "The future timestamp allowance, defaults to 7200."
    )
    (
        "bitcoin.initial_block_subsidy_bitcoin",
        value<uint64_t>(&configured.bitcoin.initial_subsidy_bitcoin),
        "The initial block subsidy, defaults to 50."
    )
    (
        "bitcoin.subsidy_interval",
        value<uint64_t>(&configured.bitcoin.subsidy_interval_blocks),
        "The subsidy halving period, defaults to 210000."
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
    // [version properties excluded here]
    (
        "bitcoin.activation_threshold",
        value<size_t>(&configured.bitcoin.activation_threshold),
        "The number of new version blocks required for bip34 style soft fork activation, defaults to 750."
    )
    (
        "bitcoin.enforcement_threshold",
        value<size_t>(&configured.bitcoin.enforcement_threshold),
        "The number of new version blocks required for bip34 style soft fork enforcement, defaults to 950."
    )
    (
        "bitcoin.activation_sample",
        value<size_t>(&configured.bitcoin.activation_sample),
        "The number of blocks considered for bip34 style soft fork activation, defaults to 1000."
    )
    (
        "bitcoin.bip65_freeze",
        value<size_t>(&configured.bitcoin.bip65_freeze),
        "The block height to freeze the bip65 softfork as in bip90, defaults to 388381."
    )
    (
        "bitcoin.bip66_freeze",
        value<size_t>(&configured.bitcoin.bip66_freeze),
        "The block height to freeze the bip66 softfork as in bip90, defaults to 363725."
    )
    (
        "bitcoin.bip34_freeze",
        value<size_t>(&configured.bitcoin.bip34_freeze),
        "The block height to freeze the bip34 softfork as in bip90, defaults to 227931."
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
        "bitcoin.bip9_bit1_active_checkpoint",
        value<chain::checkpoint>(&configured.bitcoin.bip9_bit1_active_checkpoint),
        "The hash:height checkpoint for bip9 bit1 activation, defaults to 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893:481824."
    )

    /* [network] */
    (
        "network.threads",
        value<uint32_t>(&configured.network.threads),
        "The minimum number of threads in the network threadpool, defaults to 0 (physical cores)."
    )
    (
        "network.address_maximum",
        value<uint16_t>(&configured.network.address_maximum),
        "The maximum network protocol version, defaults to 70013."
    )
    (
        "network.address_minimum",
        value<uint16_t>(&configured.network.address_minimum),
        "The maximum network protocol version, defaults to 70013."
    )
    (
        "network.protocol_maximum",
        value<uint32_t>(&configured.network.protocol_maximum),
        "The maximum network protocol version, defaults to 70013."
    )
    (
        "network.protocol_minimum",
        value<uint32_t>(&configured.network.protocol_minimum),
        "The minimum network protocol version, defaults to 31402."
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
        "network.enable_reject",
        value<bool>(&configured.network.enable_reject),
        "Enable reject messages, defaults to false."
    )
    (
        "network.enable_alert",
        value<bool>(&configured.network.enable_alert),
        "Enable alert messages, defaults to false."
    )
    (
        "network.enable_relay",
        value<bool>(&configured.network.enable_relay),
        "Enable transaction relay, defaults to true."
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
        "network.inbound_port",
        value<uint16_t>(&configured.network.inbound_port),
        "The port for incoming connections, defaults to 8333."
    )
    (
        "network.inbound_connections",
        value<uint32_t>(&configured.network.inbound_connections),
        "The target number of incoming network connections, defaults to 0."
    )
    (
        "network.outbound_connections",
        value<uint32_t>(&configured.network.outbound_connections),
        "The target number of outgoing network connections, defaults to 10."
    )
    (
        "network.connect_batch_size",
        value<uint32_t>(&configured.network.connect_batch_size),
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
        "network.channel_handshake_seconds",
        value<uint32_t>(&configured.network.channel_handshake_seconds),
        "The time limit to complete the connection handshake, defaults to 30."
    )
    (
        "network.channel_germination_seconds",
        value<uint32_t>(&configured.network.channel_germination_seconds),
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
        "The inactivity time limit for any connection, defaults to 30."
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
        "network.self",
        value<network::config::authority>(&configured.network.self),
        "The advertised public address of this node, defaults to none."
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

    /* [blockchain] */
    (
        "blockchain.cores",
        value<uint32_t>(&configured.chain.cores),
        "The number of cores dedicated to block validation, defaults to 0 (physical cores)."
    )
    (
        "blockchain.priority",
        value<bool>(&configured.chain.priority),
        "Use high thread priority for block validation, defaults to true."
    )
    (
        "blockchain.use_libconsensus",
        value<bool>(&configured.chain.use_libconsensus),
        "Use libconsensus for script validation if integrated, defaults to false."
    )
    (
        "blockchain.reorganization_limit",
        value<uint32_t>(&configured.chain.reorganization_limit),
        "The maximum reorganization depth, defaults to 0 (unlimited)."
    )
    (
        "blockchain.block_buffer_limit",
        value<uint32_t>(&configured.chain.block_buffer_limit),
        "The maximum number of blocks to buffer, defaults to 0 (none)."
    )
    (
        "blockchain.checkpoint",
        value<chain::checkpoints>(&configured.chain.checkpoints),
        "A hash:height checkpoint, multiple entries allowed."
    )
    (
        "blockchain.bip158",
        value<bool>(&configured.chain.bip158),
        "Neutrino filter (bip158 basic filter) support, defaults to false."
    )

    /* [fork] */
    (
        "fork.difficult",
        value<bool>(&configured.chain.difficult),
        "Require difficult blocks, defaults to true (use false for testnet)."
    )
    (
        "fork.retarget",
        value<bool>(&configured.chain.retarget),
        "Retarget difficulty, defaults to true."
    )
    (
        "fork.bip16",
        value<bool>(&configured.chain.bip16),
        "Add pay-to-script-hash processing, defaults to true (soft fork)."
    )
    (
        "fork.bip30",
        value<bool>(&configured.chain.bip30),
        "Disallow collision of unspent transaction hashes, defaults to true (soft fork)."
    )
    (
        "fork.bip34",
        value<bool>(&configured.chain.bip34),
        "Require coinbase input includes block height, defaults to true (soft fork)."
    )
    (
        "fork.bip42",
        value<bool>(&configured.chain.bip42),
        "Finite monetary supply, defaults to true (soft fork)."
    )
    (
        "fork.bip66",
        value<bool>(&configured.chain.bip66),
        "Require strict signature encoding, defaults to true (soft fork)."
    )
    (
        "fork.bip65",
        value<bool>(&configured.chain.bip65),
        "Add check-locktime-verify op code, defaults to true (soft fork)."
    )
    (
        "fork.bip90",
        value<bool>(&configured.chain.bip90),
        "Assume bip34, bip65, and bip66 activation if enabled, defaults to true (hard fork)."
    )
    (
        "fork.bip68",
        value<bool>(&configured.chain.bip68),
        "Add relative locktime enforcement, defaults to true (soft fork)."
    )
    (
        "fork.bip112",
        value<bool>(&configured.chain.bip112),
        "Add check-sequence-verify op code, defaults to true (soft fork)."
    )
    (
        "fork.bip113",
        value<bool>(&configured.chain.bip113),
        "Use median time past for locktime, defaults to true (soft fork)."
    )
    (
        "fork.bip141",
        value<bool>(&configured.chain.bip141),
        "Segregated witness consensus layer, defaults to true (soft fork)."
    )
    (
        "fork.bip143",
        value<bool>(&configured.chain.bip143),
        "Version 0 transaction digest, defaults to true (soft fork)."
    )
    (
        "fork.bip147",
        value<bool>(&configured.chain.bip147),
        "Prevent dummy value malleability, defaults to true (soft fork)."
    )
    (
        "fork.time_warp_patch",
        value<bool>(&configured.chain.time_warp_patch),
        "Fix time warp bug, defaults to false (hard fork)."
    )
    (
        "fork.retarget_overflow_patch",
        value<bool>(&configured.chain.retarget_overflow_patch),
        "Fix target overflow for very low difficulty, defaults to false (hard fork)."
    )
    (
        "fork.scrypt_proof_of_work",
        value<bool>(&configured.chain.scrypt_proof_of_work),
        "Use scrypt hashing for proof of work, defaults to false (hard fork)."
    )

    /* [node] */
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.notify_limit_hours",
        value<uint32_t>(&configured.chain.notify_limit_hours),
        "Disable relay when top block age exceeds, defaults to 24 (0 disables)."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.byte_fee_satoshis",
        value<float>(&configured.chain.byte_fee_satoshis),
        "The minimum fee per byte, cumulative for conflicts, defaults to 1."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.sigop_fee_satoshis",
        value<float>(&configured.chain.sigop_fee_satoshis),
        "The minimum fee per sigop, additional to byte fee, defaults to 100."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.minimum_output_satoshis",
        value<uint64_t>(&configured.chain.minimum_output_satoshis),
        "The minimum output value, defaults to 500."
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
