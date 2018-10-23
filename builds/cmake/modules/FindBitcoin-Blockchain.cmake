###############################################################################
#  Copyright (c) 2014-2018 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
# FindBitcoin-Blockchain
#
# Use this module by invoking find_package with the form::
#
#   find_package( Bitcoin-Blockchain
#     [version]              # Minimum version
#     [REQUIRED]             # Fail with error if bitcoin-blockchain is not found
#   )
#
#   Defines the following for use:
#
#   bitcoin_blockchain_FOUND              - true if headers and requested libraries were found
#   bitcoin_blockchain_INCLUDE_DIRS       - include directories for bitcoin-blockchain libraries
#   bitcoin_blockchain_LIBRARY_DIRS       - link directories for bitcoin-blockchain libraries
#   bitcoin_blockchain_LIBRARIES          - bitcoin-blockchain libraries to be linked
#   bitcoin_blockchain_PKG                - bitcoin-blockchain pkg-config package specification.
#

if (MSVC)
    if ( Bitcoin-Blockchain_FIND_REQUIRED )
        set( _bitcoin_blockchain_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _bitcoin_blockchain_MSG_STATUS "STATUS" )
    endif()

    set( bitcoin_blockchain_FOUND false )
    message( ${_bitcoin_blockchain_MSG_STATUS} "MSVC environment detection for 'bitcoin-blockchain' not currently supported." )
else ()
    # required
    if ( Bitcoin-Blockchain_FIND_REQUIRED )
        set( _bitcoin_blockchain_REQUIRED "REQUIRED" )
    endif()

    # quiet
    if ( Bitcoin-Blockchain_FIND_QUIETLY )
        set( _bitcoin_blockchain_QUIET "QUIET" )
    endif()

    # modulespec
    if ( Bitcoin-Blockchain_FIND_VERSION_COUNT EQUAL 0 )
        set( _bitcoin_blockchain_MODULE_SPEC "libbitcoin-blockchain" )
    else ()
        if ( Bitcoin-Blockchain_FIND_VERSION_EXACT )
            set( _bitcoin_blockchain_MODULE_SPEC_OP "=" )
        else ()
            set( _bitcoin_blockchain_MODULE_SPEC_OP ">=" )
        endif()

        set( _bitcoin_blockchain_MODULE_SPEC "libbitcoin-blockchain ${_bitcoin_blockchain_MODULE_SPEC_OP} ${Bitcoin-Blockchain_FIND_VERSION}" )
    endif()

    pkg_check_modules( bitcoin_blockchain ${_bitcoin_blockchain_REQUIRED} ${_bitcoin_blockchain_QUIET} "${_bitcoin_blockchain_MODULE_SPEC}" )
    set( bitcoin_blockchain_PKG "${_bitcoin_blockchain_MODULE_SPEC}" )
endif()
