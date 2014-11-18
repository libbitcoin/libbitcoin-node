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
    ${bitcoin_CFLAGS} \
    ${bitcoin_blockchain_CFLAGS}

src_libbitcoin_node_la_SOURCES = \
    src/getx_responder.cpp \
    src/poller.cpp \
    src/session.cpp \
    src/transaction_indexer.cpp

src_libbitcoin_node_la_LIBADD = \
    ${bitcoin_LIBS} \
    ${bitcoin_blockchain_LIBS}
    
#
# examples
#
# TODO

#
# tests
#
# TODO

#if WITH_TESTS
#
#if SET_BOOST_TEST_DYN_LINK
#test_libbitcoin_node_test_CPPFLAGS += \
#    -DBOOST_TEST_DYN_LINK
#endif
#
#endif
