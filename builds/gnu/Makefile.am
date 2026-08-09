###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the 'm4' subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4

# Distribute data.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-node.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    ../../COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# Libraries.
#==============================================================================
# Target library 'src/libbitcoin-node.la'
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-node.la

src_libbitcoin_node_la_LIBS = src/libbitcoin-node.la

src_libbitcoin_node_la_CPPFLAGS = \
    -I${srcdir}/../../include \
    ${libbitcoin_database_BUILD_CPPFLAGS} \
    ${libbitcoin_network_BUILD_CPPFLAGS}

src_libbitcoin_node_la_LDFLAGS = \
    ${libbitcoin_database_LDFLAGS} \
    ${libbitcoin_network_LDFLAGS}

src_libbitcoin_node_la_LIBADD = \
    ${libbitcoin_database_LIBS} \
    ${libbitcoin_network_LIBS}

src_libbitcoin_node_la_SOURCES = \
    ${srcdir}/../../src/block_arena.cpp \
    ${srcdir}/../../src/block_memory.cpp \
    ${srcdir}/../../src/configuration.cpp \
    ${srcdir}/../../src/error.cpp \
    ${srcdir}/../../src/estimator.cpp \
    ${srcdir}/../../src/full_node.cpp \
    ${srcdir}/../../src/settings.cpp \
    ${srcdir}/../../src/validate.cpp \
    ${srcdir}/../../src/channels/channel_peer.cpp \
    ${srcdir}/../../src/chasers/chaser.cpp \
    ${srcdir}/../../src/chasers/chaser_block.cpp \
    ${srcdir}/../../src/chasers/chaser_check.cpp \
    ${srcdir}/../../src/chasers/chaser_confirm.cpp \
    ${srcdir}/../../src/chasers/chaser_estimate.cpp \
    ${srcdir}/../../src/chasers/chaser_header.cpp \
    ${srcdir}/../../src/chasers/chaser_snapshot.cpp \
    ${srcdir}/../../src/chasers/chaser_storage.cpp \
    ${srcdir}/../../src/chasers/chaser_template.cpp \
    ${srcdir}/../../src/chasers/chaser_transaction.cpp \
    ${srcdir}/../../src/chasers/chaser_validate.cpp \
    ${srcdir}/../../src/chasers/chaser_validate_batch.cpp \
    ${srcdir}/../../src/chasers/chaser_validate_capture.cpp \
    ${srcdir}/../../src/chasers/chaser_validate_parallel.cpp \
    ${srcdir}/../../src/messages/block.cpp \
    ${srcdir}/../../src/messages/transaction.cpp \
    ${srcdir}/../../src/protocols/protocol.cpp \
    ${srcdir}/../../src/protocols/protocol_block_in_106.cpp \
    ${srcdir}/../../src/protocols/protocol_block_in_31800.cpp \
    ${srcdir}/../../src/protocols/protocol_block_out_106.cpp \
    ${srcdir}/../../src/protocols/protocol_block_out_70012.cpp \
    ${srcdir}/../../src/protocols/protocol_filter_out_70015.cpp \
    ${srcdir}/../../src/protocols/protocol_header_in_31800.cpp \
    ${srcdir}/../../src/protocols/protocol_header_in_70012.cpp \
    ${srcdir}/../../src/protocols/protocol_header_out_31800.cpp \
    ${srcdir}/../../src/protocols/protocol_header_out_70012.cpp \
    ${srcdir}/../../src/protocols/protocol_observer.cpp \
    ${srcdir}/../../src/protocols/protocol_peer.cpp \
    ${srcdir}/../../src/protocols/protocol_performer.cpp \
    ${srcdir}/../../src/protocols/protocol_transaction_in_106.cpp \
    ${srcdir}/../../src/protocols/protocol_transaction_out_106.cpp \
    ${srcdir}/../../src/sessions/session.cpp \
    ${srcdir}/../../src/sessions/session_inbound.cpp \
    ${srcdir}/../../src/sessions/session_manual.cpp \
    ${srcdir}/../../src/sessions/session_outbound.cpp

include_bitcoindir = \
    ${includedir}/bitcoin

include_bitcoin_HEADERS = \
    ${srcdir}/../../include/bitcoin/node.hpp

include_bitcoin_nodedir = \
    ${includedir}/bitcoin/node

include_bitcoin_node_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/block_arena.hpp \
    ${srcdir}/../../include/bitcoin/node/block_memory.hpp \
    ${srcdir}/../../include/bitcoin/node/chase.hpp \
    ${srcdir}/../../include/bitcoin/node/configuration.hpp \
    ${srcdir}/../../include/bitcoin/node/define.hpp \
    ${srcdir}/../../include/bitcoin/node/error.hpp \
    ${srcdir}/../../include/bitcoin/node/estimator.hpp \
    ${srcdir}/../../include/bitcoin/node/events.hpp \
    ${srcdir}/../../include/bitcoin/node/full_node.hpp \
    ${srcdir}/../../include/bitcoin/node/settings.hpp \
    ${srcdir}/../../include/bitcoin/node/validate.hpp \
    ${srcdir}/../../include/bitcoin/node/version.hpp

include_bitcoin_node_channelsdir = \
    ${includedir}/bitcoin/node/channels

include_bitcoin_node_channels_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/channels/channel.hpp \
    ${srcdir}/../../include/bitcoin/node/channels/channel_peer.hpp \
    ${srcdir}/../../include/bitcoin/node/channels/channels.hpp

include_bitcoin_node_chasersdir = \
    ${includedir}/bitcoin/node/chasers

include_bitcoin_node_chasers_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_block.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_check.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_confirm.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_estimate.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_header.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_organize.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_snapshot.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_storage.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_template.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_transaction.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chaser_validate.hpp \
    ${srcdir}/../../include/bitcoin/node/chasers/chasers.hpp

include_bitcoin_node_impl_chasersdir = \
    ${includedir}/bitcoin/node/impl/chasers

include_bitcoin_node_impl_chasers_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/impl/chasers/chaser_organize.ipp

include_bitcoin_node_impl_sessionsdir = \
    ${includedir}/bitcoin/node/impl/sessions

include_bitcoin_node_impl_sessions_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/impl/sessions/session_peer.ipp

include_bitcoin_node_messagesdir = \
    ${includedir}/bitcoin/node/messages

include_bitcoin_node_messages_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/messages/block.hpp \
    ${srcdir}/../../include/bitcoin/node/messages/messages.hpp \
    ${srcdir}/../../include/bitcoin/node/messages/transaction.hpp

include_bitcoin_node_protocolsdir = \
    ${includedir}/bitcoin/node/protocols

include_bitcoin_node_protocols_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_block_in_106.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_block_in_31800.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_block_out_106.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_block_out_70012.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_filter_out_70015.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_header_in_31800.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_header_in_70012.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_header_out_31800.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_header_out_70012.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_observer.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_peer.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_performer.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_transaction_in_106.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocol_transaction_out_106.hpp \
    ${srcdir}/../../include/bitcoin/node/protocols/protocols.hpp

include_bitcoin_node_sessionsdir = \
    ${includedir}/bitcoin/node/sessions

include_bitcoin_node_sessions_HEADERS = \
    ${srcdir}/../../include/bitcoin/node/sessions/session.hpp \
    ${srcdir}/../../include/bitcoin/node/sessions/session_inbound.hpp \
    ${srcdir}/../../include/bitcoin/node/sessions/session_manual.hpp \
    ${srcdir}/../../include/bitcoin/node/sessions/session_outbound.hpp \
    ${srcdir}/../../include/bitcoin/node/sessions/session_peer.hpp \
    ${srcdir}/../../include/bitcoin/node/sessions/sessions.hpp

# Tests.
#==============================================================================
# Target test 'test/libbitcoin-node-test'
#------------------------------------------------------------------------------
if WITH_TESTS

check_PROGRAMS = test/libbitcoin-node-test

test_libbitcoin_node_test_CPPFLAGS = \
    ${boost_BUILD_CPPFLAGS} \
    ${boost_unit_test_framework_BUILD_CPPFLAGS} \
    ${src_libbitcoin_node_la_CPPFLAGS}

test_libbitcoin_node_test_LDFLAGS = \
    ${boost_LDFLAGS} \
    ${boost_unit_test_framework_LDFLAGS} \
    ${src_libbitcoin_node_la_LDFLAGS}

test_libbitcoin_node_test_LDADD = \
    ${boost_LIBS} \
    ${boost_unit_test_framework_LIBS} \
    ${src_libbitcoin_node_la_LIBS} \
    ${src_libbitcoin_node_la_LIBADD}

test_libbitcoin_node_test_SOURCES = \
    ${srcdir}/../../test/block_arena.cpp \
    ${srcdir}/../../test/block_memory.cpp \
    ${srcdir}/../../test/channel_peer.cpp \
    ${srcdir}/../../test/configuration.cpp \
    ${srcdir}/../../test/error.cpp \
    ${srcdir}/../../test/estimator.cpp \
    ${srcdir}/../../test/full_node.cpp \
    ${srcdir}/../../test/main.cpp \
    ${srcdir}/../../test/settings.cpp \
    ${srcdir}/../../test/test.cpp \
    ${srcdir}/../../test/chasers/chaser.cpp \
    ${srcdir}/../../test/chasers/chaser_block.cpp \
    ${srcdir}/../../test/chasers/chaser_check.cpp \
    ${srcdir}/../../test/chasers/chaser_confirm.cpp \
    ${srcdir}/../../test/chasers/chaser_estimate.cpp \
    ${srcdir}/../../test/chasers/chaser_header.cpp \
    ${srcdir}/../../test/chasers/chaser_template.cpp \
    ${srcdir}/../../test/chasers/chaser_transaction.cpp \
    ${srcdir}/../../test/chasers/chaser_validate.cpp \
    ${srcdir}/../../test/messages/block.cpp \
    ${srcdir}/../../test/protocols/protocol.cpp \
    ${srcdir}/../../test/sessions/session.cpp

TESTS = test_runner.sh

endif WITH_TESTS
