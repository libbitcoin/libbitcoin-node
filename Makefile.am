###############################################################################
#  Copyright (c) 2014-2023 libbitcoin-node developers (see COPYING).
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
src_libbitcoin_node_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
src_libbitcoin_node_la_LIBADD = ${bitcoin_database_LIBS} ${bitcoin_network_LIBS}
src_libbitcoin_node_la_SOURCES = \
    src/block_arena.cpp \
    src/block_memory.cpp \
    src/configuration.cpp \
    src/error.cpp \
    src/full_node.cpp \
    src/parser.cpp \
    src/settings.cpp \
    src/chasers/chaser.cpp \
    src/chasers/chaser_block.cpp \
    src/chasers/chaser_check.cpp \
    src/chasers/chaser_confirm.cpp \
    src/chasers/chaser_header.cpp \
    src/chasers/chaser_snapshot.cpp \
    src/chasers/chaser_storage.cpp \
    src/chasers/chaser_template.cpp \
    src/chasers/chaser_transaction.cpp \
    src/chasers/chaser_validate.cpp \
    src/protocols/protocol.cpp \
    src/protocols/protocol_block_in.cpp \
    src/protocols/protocol_block_in_31800.cpp \
    src/protocols/protocol_block_out.cpp \
    src/protocols/protocol_header_in_31800.cpp \
    src/protocols/protocol_header_in_70012.cpp \
    src/protocols/protocol_header_out_31800.cpp \
    src/protocols/protocol_header_out_70012.cpp \
    src/protocols/protocol_observer.cpp \
    src/protocols/protocol_performer.cpp \
    src/protocols/protocol_transaction_in.cpp \
    src/protocols/protocol_transaction_out.cpp \
    src/sessions/session.cpp \
    src/sessions/session_inbound.cpp \
    src/sessions/session_manual.cpp \
    src/sessions/session_outbound.cpp

# local: test/libbitcoin-node-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-node-test_runner.sh

check_PROGRAMS = test/libbitcoin-node-test
test_libbitcoin_node_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
test_libbitcoin_node_test_LDADD = src/libbitcoin-node.la ${boost_unit_test_framework_LIBS} ${bitcoin_database_LIBS} ${bitcoin_network_LIBS}
test_libbitcoin_node_test_SOURCES = \
    test/block_arena.cpp \
    test/block_memory.cpp \
    test/configuration.cpp \
    test/error.cpp \
    test/full_node.cpp \
    test/main.cpp \
    test/node.cpp \
    test/settings.cpp \
    test/test.cpp \
    test/test.hpp \
    test/chasers/chaser.cpp \
    test/chasers/chaser_block.cpp \
    test/chasers/chaser_check.cpp \
    test/chasers/chaser_confirm.cpp \
    test/chasers/chaser_header.cpp \
    test/chasers/chaser_template.cpp \
    test/chasers/chaser_transaction.cpp \
    test/chasers/chaser_validate.cpp \
    test/protocols/protocol.cpp \
    test/sessions/session.cpp

endif WITH_TESTS

# console/bn => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bn
console_bn_CPPFLAGS = -I${srcdir}/include ${bitcoin_database_BUILD_CPPFLAGS} ${bitcoin_network_BUILD_CPPFLAGS}
console_bn_LDADD = src/libbitcoin-node.la ${bitcoin_database_LIBS} ${bitcoin_network_LIBS}
console_bn_SOURCES = \
    console/executor.cpp \
    console/executor.hpp \
    console/localize.hpp \
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
    include/bitcoin/node/block_arena.hpp \
    include/bitcoin/node/block_memory.hpp \
    include/bitcoin/node/chase.hpp \
    include/bitcoin/node/configuration.hpp \
    include/bitcoin/node/define.hpp \
    include/bitcoin/node/error.hpp \
    include/bitcoin/node/events.hpp \
    include/bitcoin/node/full_node.hpp \
    include/bitcoin/node/parser.hpp \
    include/bitcoin/node/settings.hpp \
    include/bitcoin/node/version.hpp

include_bitcoin_node_chasersdir = ${includedir}/bitcoin/node/chasers
include_bitcoin_node_chasers_HEADERS = \
    include/bitcoin/node/chasers/chaser.hpp \
    include/bitcoin/node/chasers/chaser_block.hpp \
    include/bitcoin/node/chasers/chaser_check.hpp \
    include/bitcoin/node/chasers/chaser_confirm.hpp \
    include/bitcoin/node/chasers/chaser_header.hpp \
    include/bitcoin/node/chasers/chaser_organize.hpp \
    include/bitcoin/node/chasers/chaser_snapshot.hpp \
    include/bitcoin/node/chasers/chaser_storage.hpp \
    include/bitcoin/node/chasers/chaser_template.hpp \
    include/bitcoin/node/chasers/chaser_transaction.hpp \
    include/bitcoin/node/chasers/chaser_validate.hpp \
    include/bitcoin/node/chasers/chasers.hpp

include_bitcoin_node_impl_chasersdir = ${includedir}/bitcoin/node/impl/chasers
include_bitcoin_node_impl_chasers_HEADERS = \
    include/bitcoin/node/impl/chasers/chaser_organize.ipp

include_bitcoin_node_protocolsdir = ${includedir}/bitcoin/node/protocols
include_bitcoin_node_protocols_HEADERS = \
    include/bitcoin/node/protocols/protocol.hpp \
    include/bitcoin/node/protocols/protocol_block_in.hpp \
    include/bitcoin/node/protocols/protocol_block_in_31800.hpp \
    include/bitcoin/node/protocols/protocol_block_out.hpp \
    include/bitcoin/node/protocols/protocol_header_in_31800.hpp \
    include/bitcoin/node/protocols/protocol_header_in_70012.hpp \
    include/bitcoin/node/protocols/protocol_header_out_31800.hpp \
    include/bitcoin/node/protocols/protocol_header_out_70012.hpp \
    include/bitcoin/node/protocols/protocol_observer.hpp \
    include/bitcoin/node/protocols/protocol_performer.hpp \
    include/bitcoin/node/protocols/protocol_transaction_in.hpp \
    include/bitcoin/node/protocols/protocol_transaction_out.hpp \
    include/bitcoin/node/protocols/protocols.hpp

include_bitcoin_node_sessionsdir = ${includedir}/bitcoin/node/sessions
include_bitcoin_node_sessions_HEADERS = \
    include/bitcoin/node/sessions/attach.hpp \
    include/bitcoin/node/sessions/session.hpp \
    include/bitcoin/node/sessions/session_inbound.hpp \
    include/bitcoin/node/sessions/session_manual.hpp \
    include/bitcoin/node/sessions/session_outbound.hpp \
    include/bitcoin/node/sessions/sessions.hpp

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

