###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Standard declarations.
#==============================================================================
# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4


# Automake products.
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
src_libbitcoin_node_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_blockchain_CPPFLAGS}
src_libbitcoin_node_la_LIBADD = ${bitcoin_blockchain_LIBS}
src_libbitcoin_node_la_SOURCES = \
    src/getx_responder.cpp \
    src/poller.cpp \
    src/session.cpp \
    src/transaction_indexer.cpp

# local: test/libbitcoin_node_test
#------------------------------------------------------------------------------
if WITH_TESTS

# Execute 'test/libbitcoin_node_test' as a test.
TESTS = test/libbitcoin_node_test

check_PROGRAMS = test/libbitcoin_node_test
test_libbitcoin_node_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_blockchain_CPPFLAGS}
test_libbitcoin_node_test_LDADD = src/libbitcoin-node.la ${bitcoin_blockchain_LIBS} ${boost_unit_test_framework_LIBS}
test_libbitcoin_node_test_SOURCES = \
    test/main.cpp \
    test/node.cpp

endif WITH_TESTS

# console/bitcoin_node => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bitcoin_node
console_bitcoin_node_CPPFLAGS = -I${srcdir}/include ${bitcoin_blockchain_CPPFLAGS}
console_bitcoin_node_LDADD = src/libbitcoin-node.la ${bitcoin_blockchain_LIBS}
console_bitcoin_node_SOURCES = \
    console/main.cpp

endif WITH_CONSOLE

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/node.hpp

include_bitcoin_nodedir = ${includedir}/bitcoin/node
include_bitcoin_node_HEADERS = \
    include/bitcoin/node/define.hpp \
    include/bitcoin/node/getx_responder.hpp \
    include/bitcoin/node/poller.hpp \
    include/bitcoin/node/session.hpp \
    include/bitcoin/node/transaction_indexer.hpp \
    include/bitcoin/node/version.hpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bitcoin_node

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/node.cfg

