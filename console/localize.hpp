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

#define BN_OPERATION_INTERRUPT \
    "Press CTRL-C to cancel operation."
#define BN_OPERATION_CANCELED \
    "CTRL-C detected, canceling operation..."

// --settings
#define BN_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BN_INFORMATION_MESSAGE \
    "Runs a full bitcoin node."

// --initchain
#define BN_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BN_INITCHAIN_EXISTS \
    "Failed because the directory %1% already exists."
#define BN_INITCHAIN_CREATING \
    "Please wait while creating the database..."
#define BN_INITCHAIN_COMPLETE \
    "Created and initialized empty chain in %1% secs."
#define BN_INITCHAIN_DATABASE_CREATE_FAILURE \
    "Database creation failed with error, '%1%'."
#define BN_INITCHAIN_DATABASE_INITIALIZE \
    "Database storing genesis block."
#define BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE \
    "Database failure to store genesis block."
#define BN_INITCHAIN_DATABASE_OPEN_FAILURE \
    "Database failed to open, %1%."
#define BN_INITCHAIN_DATABASE_CLOSE_FAILURE \
    "Database failed to close, %1%."

// --restore
#define BN_RESTORING_CHAIN \
    "Please wait while restoring %1% from most recent snapshot..."
#define BN_RESTORE_MISSING_FLUSH_LOCK \
    "Database is not corrupted, flush lock file is absent."
#define BN_RESTORE_INVALID \
    "Database restore disallowed with corruption, '%1%'."
#define BN_RESTORE_FAILURE \
    "Database restore failed with error, '%1%'."
#define BN_RESTORE_COMPLETE \
    "Restored database in %1% secs."

// --measure
#define BN_MEASURE_SIZES \
    "Body sizes...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   point     :%4%\n" \
    "   input     :%5%\n" \
    "   output    :%6%\n" \
    "   puts      :%7%\n" \
    "   candidate :%8%\n" \
    "   confirmed :%9%\n" \
    "   spend     :%10%\n" \
    "   strong_tx :%11%\n" \
    "   valid_tx  :%12%\n" \
    "   valid_bk  :%13%\n" \
    "   address   :%14%\n" \
    "   neutrino  :%15%"
#define BN_MEASURE_RECORDS \
    "Table records...\n" \
    "   header    :%1%\n" \
    "   tx        :%2%\n" \
    "   point     :%3%\n" \
    "   candidate :%4%\n" \
    "   confirmed :%5%\n" \
    "   spend     :%6%\n" \
    "   strong_tx :%7%\n" \
    "   address   :%8%"
#define BN_MEASURE_SLABS \
    "Table slabs..."
#define BN_MEASURE_SLABS_ROW \
    "   @tx       :%1%, inputs:%2%, outputs:%3%"
#define BN_MEASURE_STOP \
    "   input     :%1%\n" \
    "   output    :%2%\n" \
    "   seconds   :%3%"
#define BN_MEASURE_BUCKETS \
    "Head buckets...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   point     :%4%\n" \
    "   spend     :%5%\n" \
    "   strong_tx :%6%\n" \
    "   valid_tx  :%7%\n" \
    "   valid_bk  :%8%\n" \
    "   address   :%9%\n" \
    "   neutrino  :%10%"
#define BN_MEASURE_COLLISION_RATES \
    "Collision rates...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   point     :%4%\n" \
    "   spend     :%5%\n" \
    "   strong_tx :%6%\n" \
    "   valid_tx  :%7%\n" \
    "   valid_bk  :%8%\n" \
    "   address   :%9%\n" \
    "   neutrino  :%10%"
#define BN_MEASURE_PROGRESS_START \
    "Thinking..."
#define BN_MEASURE_PROGRESS \
    "Progress...\n" \
    "   fork pt   :%1%\n" \
    "   top conf  :%2%:%3%\n" \
    "   top cand  :%4%:%5%\n" \
    "   top assoc :%6%\n" \
    "   associated:%7%\n" \
    "   wire conf :%8%\n" \
    "   wire cand :%9%"

// --read
#define BN_READ_ROW \
    ": %1% in %2% secs."
#define BN_READ_ROW_MS \
    ": %1% in %2% ms."

// --write
#define BN_WRITE_ROW \
    ": %1% in %2% span."

// run/general

#define BN_CREATE \
    "create::%1%(%2%)"
#define BN_OPEN \
    "open::%1%(%2%)"
#define BN_CLOSE \
    "close::%1%(%2%)"
#define BN_BACKUP \
    "backup::%1%(%2%)"
#define BN_RESTORE \
    "restore::%1%(%2%)"
#define BN_CONDITION \
    "condition::%1%(%2%)"

#define BN_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BN_DATABASE_STARTING \
    "Please wait while the database is starting..."
#define BN_NETWORK_STARTING \
    "Please wait while the network is starting..."
#define BN_NODE_START_FAIL \
    "Node failed to start with error, %1%."
#define BN_NODE_BACKUP_UNAVAILABLE \
    "Node backup not available until started."
#define BN_NODE_BACKUP_STARTED \
    "Node backup started."
#define BN_NODE_BACKUP_FAIL \
    "Node failed to backup with error, %1%."
#define BN_NODE_BACKUP_COMPLETE \
    "Node backup complete in %1% secs."
#define BN_NODE_DISK_FULL_RESET \
    "Node reset from disk full condition."
#define BN_NODE_UNRECOVERABLE \
    "Node is not in recoverable condition."
#define BN_NODE_DISK_FULL \
    "Node cannot resume because its disk is full."
#define BN_NODE_OK \
    "Node is ok."
#define BN_NODE_STARTED \
    "Node is started."
#define BN_NODE_RUNNING \
    "Node is running."

#define BN_UNINITIALIZED_DATABASE \
    "The %1% database directory does not exist, run: bn --initchain"
#define BN_UNINITIALIZED_CHAIN \
    "The %1% database is not initialized, delete and run: bn --initchain"
#define BN_DATABASE_START_FAIL \
    "Database failed to start with error, %1%."
#define BN_DATABASE_STOPPING \
    "Please wait while the database is stopping..."
#define BN_DATABASE_STOP_FAIL \
    "Database failed to stop with error, %1%."
#define BN_DATABASE_STOPPED \
    "Database stopped successfully."
#define BN_DATABASE_TIMED_STOP \
    "Database stopped successfully in %1% secs."

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
    "libbitcoin-database:   %2%\n" \
    "libbitcoin-network:    %3%\n" \
    "libbitcoin-system:     %4%"
#define BN_LOG_HEADER \
    "====================== startup ======================="
#define BN_NODE_FOOTER \
    "====================== shutdown ======================"
#define BN_NODE_TERMINATE \
    "Press <enter> to exit..."

} // namespace node
} // namespace libbitcoin

#endif
