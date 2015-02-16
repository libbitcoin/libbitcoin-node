#
# documentation
#
doc_DATA = README INSTALL COPYING

#
# pkg-config
#
pkgconfig_DATA = libbitcoin-node.pc

#
# bash completion
#
if BASH_COMPLETION_DIR
    bashcompletiondir = ${bash_completion_dir}
    dist_bashcompletion_DATA = data/bitcoin_node
endif

#
# configuration file
#
# TODO: coordinate this dir and file name with source code to keep in sync.
# Note that in a prefix install this file is placed relative to the prefix but
# execution of the server will default to reading in the system-wide directory.
confdir="${sysconfdir}/libbitcoin"
conf_DATA = data/node.cfg

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
    include/bitcoin/node/transaction_indexer.hpp \
    include/bitcoin/node/version.hpp

#
# libraries
#
lib_LTLIBRARIES = src/libbitcoin-node.la

src_libbitcoin_node_la_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

src_libbitcoin_node_la_SOURCES = \
    src/getx_responder.cpp \
    src/poller.cpp \
    src/session.cpp \
    src/transaction_indexer.cpp

src_libbitcoin_node_la_LIBADD = \
    ${bitcoin_blockchain_LIBS}

#
# console application
#
bin_PROGRAMS = bitcoin_node

bitcoin_node_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS}

bitcoin_node_SOURCES = \
    src/console/main.cpp

bitcoin_node_LDADD = \
    src/libbitcoin-node.la \
    ${bitcoin_blockchain_LIBS}

#
# tests
#
if WITH_TESTS

TESTS = test/libbitcoin_node_test

check_PROGRAMS = test/libbitcoin_node_test

test_libbitcoin_node_test_CPPFLAGS = \
    -I${srcdir}/include \
    ${bitcoin_blockchain_CFLAGS}

test_libbitcoin_node_test_SOURCES = \
    test/main.cpp \
    test/node.cpp

test_libbitcoin_node_test_LDADD = \
    src/libbitcoin-node.la \
    -lboost_chrono \
    -lboost_program_options \
    -lboost_unit_test_framework \
    ${bitcoin_blockchain_LIBS}

endif
