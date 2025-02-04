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
    "Initializing %1% directory..."
#define BN_INITCHAIN_DIRECTORY_ERROR \
    "Failed creating directory %1% with error '%2%'."
#define BN_INITCHAIN_CREATING \
    "Please wait while creating the database..."
#define BN_INITCHAIN_CREATED \
    "Created the database in %1% secs."
#define BN_INITCHAIN_COMPLETE \
    "Created and initialized the database."
#define BN_INITCHAIN_DATABASE_CREATE_FAILURE \
    "Database creation failed with error '%1%'."
#define BN_INITCHAIN_DATABASE_INITIALIZE \
    "Storing genesis block."
#define BN_INITCHAIN_DATABASE_INITIALIZE_FAILURE \
    "Failure storing genesis block."

// --restore
#define BN_SNAPSHOT_INVALID \
    "Database snapshot disallowed due to corruption '%1%'."
#define BN_RESTORING_CHAIN \
    "Please wait while restoring from most recent snapshot..."
#define BN_RESTORE_MISSING_FLUSH_LOCK \
    "Database is not corrupted, flush lock file is absent."
#define BN_RESTORE_INVALID \
    "Database restore disallowed when corrupt '%1%'."
#define BN_RESTORE_FAILURE \
    "Database restore failed with error '%1%'."
#define BN_RESTORE_COMPLETE \
    "Restored the database in %1% secs."

// --measure
#define BN_MEASURE_SIZES \
    "Body sizes...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   input     :%4%\n" \
    "   output    :%5%\n" \
    "   puts      :%6%\n" \
    "   candidate :%7%\n" \
    "   confirmed :%8%\n" \
    "   spend     :%9%\n" \
    "   prevout   :%10%\n" \
    "   strong_tx :%11%\n" \
    "   valid_tx  :%12%\n" \
    "   valid_bk  :%13%\n" \
    "   address   :%14%\n" \
    "   neutrino  :%15%"
#define BN_MEASURE_RECORDS \
    "Table records...\n" \
    "   header    :%1%\n" \
    "   tx        :%2%\n" \
    "   candidate :%3%\n" \
    "   confirmed :%4%\n" \
    "   spend     :%5%\n" \
    "   prevout   :%6%\n" \
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
    "   spend     :%4%\n" \
    "   prevout   :%5%\n" \
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
    "   spend     :%4%\n" \
    "   prevout   :%5%\n" \
    "   strong_tx :%6%\n" \
    "   valid_tx  :%7%\n" \
    "   valid_bk  :%8%\n" \
    "   address   :%9%\n" \
    "   neutrino  :%10%"
#define BN_MEASURE_PROGRESS_START \
    "Thinking..."
#define BN_MEASURE_PROGRESS \
    "Chain progress...\n" \
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
    "snapshot::%1%(%2%)"
#define BN_RESTORE \
    "restore::%1%(%2%)"
#define BN_RELOAD \
    "reload::%1%(%2%)"
#define BN_CONDITION \
    "condition::%1%(%2%)"

#define BN_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BN_DATABASE_STARTED \
    "Database is started."
#define BN_NETWORK_STARTING \
    "Please wait while network is starting..."
#define BN_NODE_START_FAIL \
    "Node failed to start with error '%1%'."
#define BN_NODE_UNAVAILABLE \
    "Command not available until node started."

#define BN_NODE_BACKUP_STARTED \
    "Snapshot is started."
#define BN_NODE_BACKUP_FAIL \
    "Snapshot failed with error '%1%'."
#define BN_NODE_BACKUP_COMPLETE \
    "Snapshot complete in %1% secs."

#define BN_RELOAD_SPACE \
    "Free [%1%] bytes of disk space to restart."
#define BN_RELOAD_INVALID \
    "Reload disallowed due to database corruption '%1%'."
#define BN_NODE_RELOAD_STARTED \
    "Reload from disk full is started."
#define BN_NODE_RELOAD_COMPLETE \
    "Reload from disk full in %1% secs."
#define BN_NODE_RELOAD_FAIL \
    "Reload failed with error '%1%'."

#define BN_NODE_UNRECOVERABLE \
    "Node is not in recoverable condition."
#define BN_NODE_DISK_FULL \
    "Node cannot resume because its disk is full."
#define BN_NODE_OK \
    "Node is ok."

#define BN_NODE_REPORT_WORK \
    "Requested channel work report [%1%]."

#define BN_NODE_STARTED \
    "Node is started."
#define BN_NODE_RUNNING \
    "Node is running."

#define BN_UNINITIALIZED_DATABASE \
    "The %1% database directory does not exist."
#define BN_UNINITIALIZED_CHAIN \
    "The %1% database is not initialized, delete and retry."
#define BN_DATABASE_START_FAIL \
    "Database failed to start with error '%1%'."
#define BN_DATABASE_STOPPING \
    "Please wait while database is stopping..."
#define BN_DATABASE_STOP_FAIL \
    "Database failed to stop with error '%1%'."
#define BN_DATABASE_TIMED_STOP \
    "Database stopped successfully in %1% secs."

#define BN_NETWORK_STOPPING \
    "Please wait while network is stopping..."
#define BN_NODE_STOP_CODE \
    "Node stopped with code %1%."
#define BN_NODE_STOPPED \
    "Node stopped successfully."
#define BN_CHANNEL_LOG_PERIOD \
    "Log period: %1%"
#define BN_CHANNEL_STOP_TARGET \
    "Stop target: %1%"

#define BN_VERSION_MESSAGE \
    "Version Information...\n" \
    "libbitcoin-node:       %1%\n" \
    "libbitcoin-database:   %2%\n" \
    "libbitcoin-network:    %3%\n" \
    "libbitcoin-system:     %4%"

#define BN_HARDWARE_HEADER \
    "Hardware configuration..."
#define BN_HARDWARE_TABLE1 \
    "platform:%1%."
#define BN_HARDWARE_TABLE2 \
    "platform:%1% compiled:%2%."

#define BN_LOG_TABLE_HEADER \
    "Log system configuration..."
#define BN_LOG_TABLE \
    "compiled:%1% enabled:%2%."

#define BN_LOG_INITIALIZE_FAILURE \
    "Failed to initialize logging."
#define BN_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BN_USING_DEFAULT_CONFIG \
    "Using default configuration settings."
#define BN_LOG_HEADER \
    "====================== startup ======================="
#define BN_NODE_FOOTER \
    "====================== shutdown ======================"
#define BN_NODE_TERMINATE \
    "Press <enter> to exit..."

} // namespace node
} // namespace libbitcoin

#endif
