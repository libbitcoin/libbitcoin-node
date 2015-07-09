#!/bin/sh
###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-node developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Define tests and options.
#==============================================================================
BOOST_UNIT_TEST_OPTIONS=\
"--run_test=config_tests,thread_tests "\
"--show_progress=no "\
"--detect_memory_leak=0 "\
"--report_level=no "\
"--build_info=yes"


# Run tests.
#==============================================================================
./test/libbitcoin_node_test ${BOOST_UNIT_TEST_OPTIONS}
