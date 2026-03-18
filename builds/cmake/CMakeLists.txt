###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project( libbitcoin-node
  VERSION 4.0.0
  DESCRIPTION "Bitcoin Full Node"
  LANGUAGES C CXX )


set( repository_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../" )

if ( MSVC )
  set( CMAKE_STATIC_LIBRARY_PREFIX "lib" )
  set( CMAKE_SHARED_LIBRARY_PREFIX "lib" )
endif()

#------------------------------------------------------------------------------
# Project options.
#------------------------------------------------------------------------------
option( with-test "Build test." ON )

#------------------------------------------------------------------------------
# Dependencies.
#------------------------------------------------------------------------------

if ( BUILD_SHARED_LIBS )
  set( libbitcoin-database_USE_STATIC_LIBS "OFF" )
  set( libbitcoin-network_USE_STATIC_LIBS "OFF" )
  set( Boost_USE_STATIC_LIBS "OFF" )
else()
  set( libbitcoin-database_USE_STATIC_LIBS "ON" )
  set( libbitcoin-network_USE_STATIC_LIBS "ON" )
  set( Boost_USE_STATIC_LIBS "ON" )
endif()

find_package( libbitcoin-database 4.0.0 REQUIRED )

find_package( libbitcoin-network 4.0.0 REQUIRED )

find_package( Boost 1.86.0 REQUIRED
  COMPONENTS
    unit_test_framework
)

#------------------------------------------------------------------------------
# Compiler options.
#------------------------------------------------------------------------------

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

check_cxx_compiler_flag( "-Wall" HAS_FLAG_WALL )
if ( !HAS_FLAG_WALL )
    message( FATAL_ERROR "Compiler does not support '-Wall'." )
endif()

check_cxx_compiler_flag( "-Wextra" HAS_FLAG_WEXTRA )
if ( !HAS_FLAG_WEXTRA )
    message( FATAL_ERROR "Compiler does not support '-Wextra'." )
endif()

check_cxx_compiler_flag( "-Wno-reorder" HAS_FLAG_WNO-REORDER )
if ( !HAS_FLAG_WNO-REORDER )
    message( FATAL_ERROR "Compiler does not support '-Wno-reorder'." )
endif()

check_cxx_compiler_flag( "-Wno-missing-field-initializers" HAS_FLAG_WNO-MISSING-FIELD-INITIALIZERS )
if ( !HAS_FLAG_WNO-MISSING-FIELD-INITIALIZERS )
    message( FATAL_ERROR "Compiler does not support '-Wno-missing-field-initializers'." )
endif()

check_cxx_compiler_flag( "-Wno-missing-braces" HAS_FLAG_WNO-MISSING-BRACES )
if ( !HAS_FLAG_WNO-MISSING-BRACES )
    message( FATAL_ERROR "Compiler does not support '-Wno-missing-braces'." )
endif()

check_cxx_compiler_flag( "-Wno-comment" HAS_FLAG_WNO-COMMENT )
if ( !HAS_FLAG_WNO-COMMENT )
    message( FATAL_ERROR "Compiler does not support '-Wno-comment'." )
endif()

check_cxx_compiler_flag( "-Wno-deprecated-copy" HAS_FLAG_WNO-DEPRECATED-COPY )
if ( !HAS_FLAG_WNO-DEPRECATED-COPY )
    message( FATAL_ERROR "Compiler does not support '-Wno-deprecated-copy'." )
endif()

if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
  check_cxx_compiler_flag( "-Wno-mismatched-tags" HAS_FLAG_WNO-MISMATCHED-TAGS )
  if ( !HAS_FLAG_WNO-MISMATCHED-TAGS )
      message( FATAL_ERROR "Compiler does not support '-Wno-mismatched-tags'." )
  endif()
endif()

check_cxx_compiler_flag( "-fstack-protector-all" HAS_FLAG_FSTACK-PROTECTOR-ALL )
if ( !HAS_FLAG_FSTACK-PROTECTOR-ALL )
    message( FATAL_ERROR "Compiler does not support '-fstack-protector-all'." )
endif()

check_cxx_compiler_flag( "-Wno-ignored-attributes" HAS_FLAG_WNO-IGNORED-ATTRIBUTES )
if ( !HAS_FLAG_WNO-IGNORED-ATTRIBUTES )
    message( FATAL_ERROR "Compiler does not support '-Wno-ignored-attributes'." )
endif()

check_cxx_compiler_flag( "-Wno-long-long" HAS_FLAG_WNO-LONG-LONG )
if ( !HAS_FLAG_WNO-LONG-LONG )
    message( FATAL_ERROR "Compiler does not support '-Wno-long-long'." )
endif()

if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  check_cxx_compiler_flag( "-fno-var-tracking-assignments" HAS_FLAG_FNO-VAR-TRACKING-ASSIGNMENTS )
  if ( !HAS_FLAG_FNO-VAR-TRACKING-ASSIGNMENTS )
      message( FATAL_ERROR "Compiler does not support '-fno-var-tracking-assignments'." )
  endif()
endif()

#------------------------------------------------------------------------------
# Linker options.
#------------------------------------------------------------------------------

include(CheckLinkerFlag)

#------------------------------------------------------------------------------
# libbitcoin-node library
#------------------------------------------------------------------------------
add_library( libbitcoin-node )
add_library( bitcoin::node ALIAS libbitcoin-node )

target_compile_features( libbitcoin-node
  PUBLIC
    cxx_std_20
)

target_compile_options( libbitcoin-node
  PRIVATE
    -Wall
    -Wextra
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-braces>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-comment>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-mismatched-tags>
    -fstack-protector-all
)

file( GLOB_RECURSE libbitcoin_node_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/*.ipp"
)

file( GLOB_RECURSE libbitcoin_node_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.cpp"
)

target_sources( libbitcoin-node
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
    FILES
      ${libbitcoin_node_HEADERS}
  PRIVATE
    ${libbitcoin_node_SOURCES}
)

target_include_directories( libbitcoin-node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries( libbitcoin-node
  PUBLIC
    bitcoin::database
    bitcoin::network
)

set_target_properties( libbitcoin-node
  PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME node
    OUTPUT_NAME bitcoin-node
)

#------------------------------------------------------------------------------
# libbitcoin-node-tests tests
#------------------------------------------------------------------------------
if ( with-test )
  add_executable( libbitcoin-node-tests )

  enable_testing()

  add_test( NAME libbitcoin-node-tests
    COMMAND libbitcoin-node-tests
    --run_test=*
    --log_level=warning
    --show_progress=no
    --detect_memory_leak=0
    --report_level=no
    --build_info=yes
  )

  target_compile_features( libbitcoin-node-tests
    PUBLIC
      cxx_std_20
  )

  target_compile_options( libbitcoin-node-tests
    PRIVATE
      -Wall
      -Wextra
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-braces>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-comment>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-ignored-attributes>
      $<$<CXX_COMPILER_ID:Clang>:-Wno-mismatched-tags>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-long-long>
      $<$<CXX_COMPILER_ID:GNU>:-fno-var-tracking-assignments>
      -fstack-protector-all
  )

  file( GLOB_RECURSE libbitcoin_node_tests_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.cpp"
  )

  target_sources( libbitcoin-node-tests
    PRIVATE
      ${libbitcoin_node_tests_SOURCES}
  )

  target_link_libraries( libbitcoin-node-tests
    PRIVATE
      Boost::unit_test_framework
      bitcoin::node
  )

  set_target_properties( libbitcoin-node-tests
    PROPERTIES
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
  )
endif()
#------------------------------------------------------------------------------
# Installation routine.
#------------------------------------------------------------------------------
include( GNUInstallDirs )
include( CMakePackageConfigHelpers )

install( TARGETS libbitcoin-node
  EXPORT libbitcoin-node-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILE_SET HEADERS
)

install( EXPORT libbitcoin-node-targets
  NAMESPACE bitcoin::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libbitcoin-node
  FILE libbitcoin-node-targets.cmake
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libbitcoin-node-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-node-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libbitcoin-node
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-node-config-version.cmake"
  VERSION ${PACKAGE_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-node-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-node-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libbitcoin-node
)
