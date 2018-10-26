###############################################################################
#  Copyright (c) 2014-2018 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# libbitcoin-node project configuration.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(libbitcoin-node LANGUAGES C CXX)

enable_testing()

list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules" )
include(CheckIncludeFiles)
include(CheckSymbolExists)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
  set( CANONICAL_LIB_NAME libbitcoin-node )
else ()
  set( CANONICAL_LIB_NAME bitcoin-node )
  find_package( PkgConfig REQUIRED )

  set( prefix "${CMAKE_PREFIX_PATH}" )
  set( exec_prefix "\${prefix}" )
  set( libdir "\${exec_prefix}/lib" )
  set( includedir "\${exec_prefix}/include" )

  set( PACKAGE_VERSION "4.0.0" )
  set( VERSION "${PACKAGE_VERSION}" )
endif ()

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Add compiler options
#------------------------------------------------------------------------------
# Warn on all stuff.
add_compile_options( "-Wall" )

# Warn on extra stuff.
add_compile_options( "-Wextra" )

# Be really annoying.
add_compile_options( "-Wpedantic" )

# Disallow warning on style order of declarations.
add_compile_options( "-Wno-reorder" )

# Suppress warning for incomplete field initialization.
add_compile_options( "-Wno-missing-field-initializers" )

# Conform to style.
add_compile_options( "-Wno-missing-braces" )

# Ignore comments within comments or commenting of backslash extended lines.
add_compile_options( "-Wno-comment" )

# Conflict in stdlib under clang.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options( "-Wno-mismatched-tags" )
endif()

# Implement -Dbash-completiondir and output ${bash-completiondir} and declare bash-completiondir.
#------------------------------------------------------------------------------
set( bash-completiondir "no" CACHE BOOL "Install bash completion support, optionally specifying the directory. This option may require elevated permissions." )

# Implement -Dpkgconfigdir and output ${pkgconfigdir}.
#------------------------------------------------------------------------------
set( pkgconfigdir "${libdir}/pkgconfig" CACHE PATH "Path to pkgconfig directory." )

# Implement -Dwith-tests and declare with-tests.
#------------------------------------------------------------------------------
set( with-tests "yes" CACHE BOOL "Compile with unit tests." )

# Implement -Dwith-console and declare with-console.
#------------------------------------------------------------------------------
set( with-console "yes" CACHE BOOL "Compile console application." )

# Implement -Denable-ndebug and define NDEBUG.
#------------------------------------------------------------------------------
set( enable-ndebug "yes" CACHE BOOL "Compile without debug assertions." )

if (enable-ndebug)
    add_definitions( -DNDEBUG )
endif()

# Inherit -Denable-shared and define BOOST_ALL_DYN_LINK.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    add_definitions( -DBOOST_ALL_DYN_LINK )
endif()

# Find boost
#------------------------------------------------------------------------------
find_package( Boost 1.62.0 REQUIRED COMPONENTS
    unit_test_framework )

set( boost_unit_test_framework_LIBS "-lboost_unit_test_framework" )

if (enable-ndebug)
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_DEBUG}" )
else ()
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_RELEASE}" )
endif()

set( boost_CPPFLAGS "-I${Boost_INCLUDE_DIR}" )
set( boost_LDFLAGS "-L${Boost_LIBRARY_DIR}" )

# Find bash-completion
#------------------------------------------------------------------------------
if (bash-completiondir)
    find_package( Bash-Completion 2.0.0 REQUIRED )
endif()

# Find bitcoin-blockchain
#------------------------------------------------------------------------------
find_package( Bitcoin-Blockchain 4.0.0 REQUIRED )

# Find bitcoin-network
#------------------------------------------------------------------------------
find_package( Bitcoin-Network 4.0.0 REQUIRED )

# Define project common includes directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    include_directories( SYSTEM
        ${bitcoin_blockchain_INCLUDE_DIRS}
        ${bitcoin_network_INCLUDE_DIRS} )
else()
    include_directories( SYSTEM
        ${bitcoin_blockchain_STATIC_INCLUDE_DIRS}
        ${bitcoin_network_STATIC_INCLUDE_DIRS} )
endif()

# Define project common library directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_directories( 
        ${bitcoin_blockchain_LIBRARY_DIRS}
        ${bitcoin_network_LIBRARY_DIRS} )
else()
    link_directories( 
        ${bitcoin_blockchain_STATIC_LIBRARY_DIRS}
        ${bitcoin_network_STATIC_LIBRARY_DIRS} )
endif()

# Define project common libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_libraries( 
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_blockchain_LIBRARIES}
        ${bitcoin_network_LIBRARIES} )
else()
    link_libraries( 
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_blockchain_STATIC_LIBRARIES}
        ${bitcoin_network_STATIC_LIBRARIES} )
endif()

add_definitions( 
    -DSYSCONFDIR=\"${sysconfdir}\" )

# Define ${CANONICAL_LIB_NAME} project.
#------------------------------------------------------------------------------
add_library( ${CANONICAL_LIB_NAME}
    "../../src/configuration.cpp"
    "../../src/full_node.cpp"
    "../../src/parser.cpp"
    "../../src/settings.cpp"
    "../../src/protocols/protocol_block_in.cpp"
    "../../src/protocols/protocol_block_out.cpp"
    "../../src/protocols/protocol_block_sync.cpp"
    "../../src/protocols/protocol_header_in.cpp"
    "../../src/protocols/protocol_transaction_in.cpp"
    "../../src/protocols/protocol_transaction_out.cpp"
    "../../src/sessions/session_inbound.cpp"
    "../../src/sessions/session_manual.cpp"
    "../../src/sessions/session_outbound.cpp"
    "../../src/utility/check_list.cpp"
    "../../src/utility/hash_queue.cpp"
    "../../src/utility/performance.cpp"
    "../../src/utility/reservation.cpp"
    "../../src/utility/reservations.cpp" )

# ${CANONICAL_LIB_NAME} project specific include directories.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_blockchain_INCLUDE_DIRS}
        ${bitcoin_network_INCLUDE_DIRS} )
else()
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_blockchain_STATIC_INCLUDE_DIRS}
        ${bitcoin_network_STATIC_INCLUDE_DIRS} )
endif()

target_include_directories( ${CANONICAL_LIB_NAME} PUBLIC
"../../include" )

# ${CANONICAL_LIB_NAME} project specific libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_blockchain_LIBRARIES}
        ${bitcoin_network_LIBRARIES} )
else()
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_blockchain_STATIC_LIBRARIES}
        ${bitcoin_network_STATIC_LIBRARIES} )
endif()

# Define libbitcoin-node-test project.
#------------------------------------------------------------------------------
if (with-tests)
    add_executable( libbitcoin-node-test
        "../../test/check_list.cpp"
        "../../test/configuration.cpp"
        "../../test/main.cpp"
        "../../test/node.cpp"
        "../../test/performance.cpp"
        "../../test/reservation.cpp"
        "../../test/reservations.cpp"
        "../../test/settings.cpp"
        "../../test/utility.cpp"
        "../../test/utility.hpp" )

    add_test( NAME libbitcoin-node-test COMMAND libbitcoin-node-test
            --run_test=*
            --show_progress=no
            --detect_memory_leak=0
            --report_level=no
            --build_info=yes )

#     libbitcoin-node-test project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( libbitcoin-node-test PRIVATE
        "../../include" )

#     libbitcoin-node-test project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( libbitcoin-node-test
        ${CANONICAL_LIB_NAME}
        ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )

endif()

# Define bn project.
#------------------------------------------------------------------------------
if (with-console)
    add_executable( bn
        "../../console/executor.cpp"
        "../../console/executor.hpp"
        "../../console/libbitcoin.ico"
        "../../console/main.cpp" )

#     bn project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( bn PRIVATE
        "../../include" )

#     bn project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( bn
        ${CANONICAL_LIB_NAME} )

endif()

# Manage pkgconfig installation.
#------------------------------------------------------------------------------
configure_file(
  "../../libbitcoin-node.pc.in"
  "libbitcoin-node.pc" @ONLY )

install( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-node.pc"
    DESTINATION "${pkgconfigdir}" )

# Manage installation of docs.
#------------------------------------------------------------------------------
install( FILES
    "../../AUTHORS"
    "../../COPYING"
    "../../ChangeLog"
    "../../INSTALL"
    "../../NEWS"
    "../../README"
    DESTINATION share/doc/libbitcoin-node )

# Manage ${CANONICAL_LIB_NAME} installation.
#------------------------------------------------------------------------------
install( TARGETS ${CANONICAL_LIB_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include )

# Manage bn installation.
#------------------------------------------------------------------------------
if (with-console)
    install( TARGETS bn
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include )
endif()

# Manage include installation.
#------------------------------------------------------------------------------
install( DIRECTORY "../../include/bitcoin"
    DESTINATION include )

# Manage data installation for bash_completion prefixed product.
#------------------------------------------------------------------------------
if (bash-completiondir)
    install( FILES
        "../../data/bn"
        DESTINATION etc/libbitcoin )
endif()

# Manage data installation for sysconf prefixed product.
#------------------------------------------------------------------------------
if (with-console)
    install( FILES
        "../../data/bn.cfg"
        DESTINATION etc/libbitcoin )
endif()

