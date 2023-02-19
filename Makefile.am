###############################################################################
#  Copyright (c) 2014-2020 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the m4 subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4


# Distribute, make and install products.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-node.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# src/libbitcoin-node.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-node.la
src_libbitcoin_node_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_blockchain_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
src_libbitcoin_node_la_LIBADD = ${bitcoin_blockchain_LIBS} ${bitcoin_network_LIBS}
src_libbitcoin_node_la_SOURCES = \
    src/configuration.cpp \
    src/error.cpp \
    src/full_node.cpp \
    src/parser.cpp \
    src/settings.cpp \
    src/protocols/protocol.cpp \
    src/protocols/protocol_block_in.cpp \
    src/protocols/protocol_block_out.cpp \
    src/protocols/protocol_header_in.cpp \
    src/protocols/protocol_transaction_in.cpp \
    src/protocols/protocol_transaction_out.cpp

# local: test/libbitcoin-node-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-node-test_runner.sh

check_PROGRAMS = test/libbitcoin-node-test
test_libbitcoin_node_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_blockchain_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
test_libbitcoin_node_test_LDADD = src/libbitcoin-node.la ${boost_unit_test_framework_LIBS} ${bitcoin_blockchain_LIBS} ${bitcoin_network_LIBS}
test_libbitcoin_node_test_SOURCES = \
    test/configuration.cpp \
    test/error.cpp \
    test/main.cpp \
    test/node.cpp \
    test/settings.cpp \
    test/test.cpp \
    test/test.hpp

endif WITH_TESTS

# console/bn => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bn
console_bn_CPPFLAGS = -I${srcdir}/include ${bitcoin_blockchain_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
console_bn_LDADD = src/libbitcoin-node.la ${bitcoin_blockchain_LIBS} ${bitcoin_network_LIBS}
console_bn_SOURCES = \
    console/executor.cpp \
    console/executor.hpp \
    console/main.cpp \
    console/stack_trace.cpp \
    console/stack_trace.hpp

endif WITH_CONSOLE

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/node.hpp

include_bitcoin_nodedir = ${includedir}/bitcoin/node
include_bitcoin_node_HEADERS = \
    include/bitcoin/node/configuration.hpp \
    include/bitcoin/node/define.hpp \
    include/bitcoin/node/error.hpp \
    include/bitcoin/node/full_node.hpp \
    include/bitcoin/node/parser.hpp \
    include/bitcoin/node/settings.hpp \
    include/bitcoin/node/version.hpp

include_bitcoin_node_protocolsdir = ${includedir}/bitcoin/node/protocols
include_bitcoin_node_protocols_HEADERS = \
    include/bitcoin/node/protocols/protocol.hpp \
    include/bitcoin/node/protocols/protocol_block_in.hpp \
    include/bitcoin/node/protocols/protocol_block_out.hpp \
    include/bitcoin/node/protocols/protocol_header_in.hpp \
    include/bitcoin/node/protocols/protocol_transaction_in.hpp \
    include/bitcoin/node/protocols/protocol_transaction_out.hpp \
    include/bitcoin/node/protocols/protocols.hpp

include_bitcoin_node_sessionsdir = ${includedir}/bitcoin/node/sessions
include_bitcoin_node_sessions_HEADERS = \
    include/bitcoin/node/sessions/session.hpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bn

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
if WITH_CONSOLE

sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/bn.cfg

endif WITH_CONSOLE


# Custom make targets.
#==============================================================================
# make target: console
#------------------------------------------------------------------------------
target_console = \
    console/bn

console: ${target_console}

