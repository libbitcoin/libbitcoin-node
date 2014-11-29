#
# documentation
#
doc_DATA = README INSTALL COPYING

#
# pkg-config
#
pkgconfig_DATA = libbitcoin-node.pc

# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4

#
# headers
#
bitcoin_includedir = ${includedir}/bitcoin
bitcoin_include_HEADERS = \
    include/bitcoin/node.hpp
    
bitcoin_node_includedir = ${includedir}/bitcoin/node
bitcoin_node_include_HEADERS = \
    include/bitcoin/node/define.hpp \
    include/bitcoin/node/getx_responder.hpp \
    include/bitcoin/node/poller.hpp \
    include/bitcoin/node/session.hpp \
    include/bitcoin/node/transaction_indexer.hpp

#
# libraries
#
lib_LTLIBRARIES = src/libbitcoin-node.la

src_libbitcoin_node_la_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS}

src_libbitcoin_node_la_SOURCES = \
    src/getx_responder.cpp \
    src/poller.cpp \
    src/session.cpp \
    src/transaction_indexer.cpp

src_libbitcoin_node_la_LIBADD = \
    ${bitcoin_blockchain_LIBS}

#
# examples
#
EXTRA_PROGRAMS = examples/fullnode

examples_fullnode_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS}

examples_fullnode_LDADD = \
    src/libbitcoin-node.la \
    ${bitcoin_blockchain_LIBS}

examples_fullnode_SOURCES = \
    examples/fullnode.cpp

examples: examples/fullnode

#
# tests
#
if WITH_TESTS
TESTS = test/libbitcoin_node_test

check_PROGRAMS = test/libbitcoin_node_test

test_libbitcoin_node_test_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS}

if ENABLE_SHARED 
test_libbitcoin_node_test_CPPFLAGS += \
    -DBOOST_TEST_DYN_LINK
endif

test_libbitcoin_node_test_SOURCES = \
    test/main.cpp \
    test/node.cpp

test_libbitcoin_node_test_LDADD = \
    src/libbitcoin-node.la \
    -lboost_unit_test_framework \
    ${bitcoin_blockchain_LIBS}
endif
