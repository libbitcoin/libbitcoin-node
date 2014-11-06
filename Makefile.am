#
# documentation
#
# doc_DATA = README.md INSTALL COPYING doc/README.tutorial

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
    ${bitcoin_CFLAGS}

src_libbitcoin_node_la_SOURCES = \
    src/getx_responder.cpp \
    src/poller.cpp \
    src/session.cpp \
    src/transaction_indexer.cpp

src_libbitcoin_node_la_LIBADD = \
    ${bitcoin_LIBS}
    
#
# examples
#
# TODO

#
# tests
#
# TODO
if WITH_TESTS
endif
