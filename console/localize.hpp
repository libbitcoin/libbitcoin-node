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
#ifndef LIBBITCOIN_NODE_LOCALIZE_HPP
#define LIBBITCOIN_NODE_LOCALIZE_HPP

/// Localizable messages.

namespace libbitcoin {
namespace node {

// --settings
#define BN_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BN_INFORMATION_MESSAGE \
    "Runs a full bitcoin node with additional client-server query protocol."

// --initchain
#define BN_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BN_INITCHAIN_EXISTS \
    "Failed because the directory %1% already exists."
#define BN_INITCHAIN_CREATING \
    "Please wait while creating the store..."
#define BN_INITCHAIN_COMPLETE \
    "Created and initialized empty chain in %1% ms."
#define BN_INITCHAIN_DATABASE_CREATE_FAILURE \
    "Database creation failed with error, '%1%'."
#define BN_INITCHAIN_DATABASE_INITIALIZE \
    "Database storing genesis block."
#define BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE \
    "Database failure to store genesis block."
#define BN_INITCHAIN_DATABASE_OPEN_FAILURE \
    "Database failure to open, %1%."
#define BN_INITCHAIN_DATABASE_CLOSE_FAILURE \
    "Database failure to close, %1%."

// --totals
#define BN_TOTALS_RECORDS \
    "Table records...\n" \
    "   header :%1%\n"  \
    "   tx     :%2%\n"\
    "   point  :%3%\n" \
    "   puts   :%4%"
#define BN_TOTALS_SIZES \
    "Body sizes...\n" \
    "   header :%1%\n" \
    "   txs    :%2%\n" \
    "   tx     :%3%\n" \
    "   point  :%4%\n" \
    "   puts   :%5%\n" \
    "   input  :%6%\n" \
    "   output :%7%"
#define BN_TOTALS_SLABS \
    "Table slabs..."
#define BN_TOTALS_SLABS_ROW \
    "   @tx    :%1%, inputs:%2%, outputs:%3%"
#define BN_TOTALS_STOP \
    "   seconds:%1%\n" \
    "   input  :%2%\n" \
    "   output :%3%"
#define BN_TOTALS_COLLISION_RATES \
    "Head buckets...\n" \
    "   header :%1% (%2%)\n" \
    "   txs    :%3% (%4%)\n" \
    "   tx     :%5% (%6%)\n" \
    "   point  :%7% (%8%)\n" \
    "   input  :%9% (%10%)"
#define BN_TOTALS_BUCKETS \
    "Head buckets...\n" \
    "   header :%1%\n" \
    "   txs    :%2%\n" \
    "   tx     :%3%\n" \
    "   point  :%4%\n" \
    "   input  :%5%"
#define BN_TOTALS_INTERRUPT \
    "Press CTRL-C to cancel."
#define BN_TOTALS_CANCELED \
    "CTRL-C detected, stopping..."

// run/general

#define BN_CREATE \
    "create::%1%(%2%)"
#define BN_OPEN \
    "open::%1%(%2%)"
#define BN_CLOSE \
    "close::%1%(%2%)"

#define BN_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BN_STORE_STARTING \
    "Please wait while the store is starting..."
#define BN_NETWORK_STARTING \
    "Please wait while the network is starting..."
#define BN_NODE_START_FAIL \
    "Node failed to start with error, %1%."
#define BN_NODE_STARTED \
    "Node is started."
#define BN_NODE_RUNNING \
    "Node is running."

#define BN_UNINITIALIZED_STORE \
    "The %1% store directory does not exist, run: bn --initchain"
#define BN_UNINITIALIZED_CHAIN \
    "The %1% store is not initialized, delete and run: bn --initchain"
#define BN_STORE_START_FAIL \
    "Store failed to start with error, %1%."
#define BN_STORE_STOPPING \
    "Please wait while the store is stopping..."
#define BN_STORE_STOP_FAIL \
    "Store failed to stop with error, %1%."
#define BN_STORE_STOPPED \
    "Store stopped successfully."

#define BN_NETWORK_STOPPING \
    "Please wait while the network is stopping..."
#define BN_NODE_STOP_CODE \
    "Node stopped with code, %1%."
#define BN_NODE_STOPPED \
    "Node stopped successfully."
#define BN_CHANNEL_LOG_PERIOD \
    "Log period: %1%"
#define BN_CHANNEL_STOP_TARGET \
    "Stop target: %1%"

#define BN_LOG_INITIALIZE_FAILURE \
    "Failed to initialize logging."
#define BN_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BN_USING_DEFAULT_CONFIG \
    "Using default configuration settings."
#define BN_VERSION_MESSAGE \
    "\nVersion Information\n" \
    "----------------------------\n" \
    "libbitcoin-node:       %1%\n" \
    "libbitcoin-blockchain: %2%\n" \
    "libbitcoin-database:   %3%\n" \
    "libbitcoin-network:    %4%\n" \
    "libbitcoin-system:     %5%"
#define BN_LOG_HEADER \
    "====================== startup ======================="
#define BN_NODE_FOOTER \
    "====================== shutdown ======================"
#define BN_NODE_TERMINATE \
    "Press <enter> to exit..."

} // namespace node
} // namespace libbitcoin

#endif
