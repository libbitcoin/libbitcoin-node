###############################################################################
#  Copyright (c) 2014-2023 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
# FindBitcoin-Consensus
#
# Use this module by invoking find_package with the form::
#
#   find_package( Bitcoin-Consensus
#     [version]              # Minimum version
#     [REQUIRED]             # Fail with error if bitcoin-consensus is not found
#   )
#
#   Defines the following for use:
#
#   bitcoin_consensus_FOUND               - true if headers and requested libraries were found
#   bitcoin_consensus_INCLUDE_DIRS        - include directories for bitcoin-consensus libraries
#   bitcoin_consensus_LIBRARY_DIRS        - link directories for bitcoin-consensus libraries
#   bitcoin_consensus_LIBRARIES           - bitcoin-consensus libraries to be linked
#   bitcoin_consensus_PKG                 - bitcoin-consensus pkg-config package specification.
#

if (MSVC)
    if ( Bitcoin-Consensus_FIND_REQUIRED )
        set( _bitcoin_consensus_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _bitcoin_consensus_MSG_STATUS "STATUS" )
    endif()

    set( bitcoin_consensus_FOUND false )
    message( ${_bitcoin_consensus_MSG_STATUS} "MSVC environment detection for 'bitcoin-consensus' not currently supported." )
else ()
    # required
    if ( Bitcoin-Consensus_FIND_REQUIRED )
        set( _bitcoin_consensus_REQUIRED "REQUIRED" )
    endif()

    # quiet
    if ( Bitcoin-Consensus_FIND_QUIETLY )
        set( _bitcoin_consensus_QUIET "QUIET" )
    endif()

    # modulespec
    if ( Bitcoin-Consensus_FIND_VERSION_COUNT EQUAL 0 )
        set( _bitcoin_consensus_MODULE_SPEC "libbitcoin-consensus" )
    else ()
        if ( Bitcoin-Consensus_FIND_VERSION_EXACT )
            set( _bitcoin_consensus_MODULE_SPEC_OP "=" )
        else ()
            set( _bitcoin_consensus_MODULE_SPEC_OP ">=" )
        endif()

        set( _bitcoin_consensus_MODULE_SPEC "libbitcoin-consensus ${_bitcoin_consensus_MODULE_SPEC_OP} ${Bitcoin-Consensus_FIND_VERSION}" )
    endif()

    pkg_check_modules( bitcoin_consensus ${_bitcoin_consensus_REQUIRED} ${_bitcoin_consensus_QUIET} "${_bitcoin_consensus_MODULE_SPEC}" )
    set( bitcoin_consensus_PKG "${_bitcoin_consensus_MODULE_SPEC}" )
endif()
