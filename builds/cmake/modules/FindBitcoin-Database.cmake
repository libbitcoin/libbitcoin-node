###############################################################################
#  Copyright (c) 2014-2023 libbitcoin-blockchain developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
# FindBitcoin-Database
#
# Use this module by invoking find_package with the form::
#
#   find_package( Bitcoin-Database
#     [version]              # Minimum version
#     [REQUIRED]             # Fail with error if bitcoin-database is not found
#   )
#
#   Defines the following for use:
#
#   bitcoin_database_FOUND                - true if headers and requested libraries were found
#   bitcoin_database_INCLUDE_DIRS         - include directories for bitcoin-database libraries
#   bitcoin_database_LIBRARY_DIRS         - link directories for bitcoin-database libraries
#   bitcoin_database_LIBRARIES            - bitcoin-database libraries to be linked
#   bitcoin_database_PKG                  - bitcoin-database pkg-config package specification.
#

if (MSVC)
    if ( Bitcoin-Database_FIND_REQUIRED )
        set( _bitcoin_database_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _bitcoin_database_MSG_STATUS "STATUS" )
    endif()

    set( bitcoin_database_FOUND false )
    message( ${_bitcoin_database_MSG_STATUS} "MSVC environment detection for 'bitcoin-database' not currently supported." )
else ()
    # required
    if ( Bitcoin-Database_FIND_REQUIRED )
        set( _bitcoin_database_REQUIRED "REQUIRED" )
    endif()

    # quiet
    if ( Bitcoin-Database_FIND_QUIETLY )
        set( _bitcoin_database_QUIET "QUIET" )
    endif()

    # modulespec
    if ( Bitcoin-Database_FIND_VERSION_COUNT EQUAL 0 )
        set( _bitcoin_database_MODULE_SPEC "libbitcoin-database" )
    else ()
        if ( Bitcoin-Database_FIND_VERSION_EXACT )
            set( _bitcoin_database_MODULE_SPEC_OP "=" )
        else ()
            set( _bitcoin_database_MODULE_SPEC_OP ">=" )
        endif()

        set( _bitcoin_database_MODULE_SPEC "libbitcoin-database ${_bitcoin_database_MODULE_SPEC_OP} ${Bitcoin-Database_FIND_VERSION}" )
    endif()

    pkg_check_modules( bitcoin_database ${_bitcoin_database_REQUIRED} ${_bitcoin_database_QUIET} "${_bitcoin_database_MODULE_SPEC}" )
    set( bitcoin_database_PKG "${_bitcoin_database_MODULE_SPEC}" )
endif()
