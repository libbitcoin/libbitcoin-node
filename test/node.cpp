/**
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-protocol.
 *
 * libbitcoin-protocol is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <bitcoin/node.hpp>

static void uninitchain(const char prefix[])
{
    boost::filesystem::remove_all(prefix);
}

static void initchain(const char prefix[])
{
    const size_t history_height = 0;
    const auto genesis = bc::genesis_block();

    uninitchain(prefix);
    boost::filesystem::create_directories(prefix);
    bc::chain::initialize_blockchain(prefix);

    bc::chain::db_paths paths(prefix);
    bc::chain::db_interface interface(paths, { history_height });

    interface.start();
    interface.push(genesis);
}

// TODO: move construction expressions into BOOST_REQUIRE_NO_THROW.
BOOST_AUTO_TEST_SUITE(node_tests)

BOOST_AUTO_TEST_CASE(node_test__construct_getx_responder__does_not_throw)
{
    // WARNING: file system side effect, use unique relative path.
    const static auto prefix = "node_test/construct_getx_responder";
    initchain(prefix);

    bc::threadpool threads;
    bc::chain::blockchain_impl blockchain(threads, prefix);
    bc::chain::transaction_pool transactions(threads, blockchain);
    bc::node::getx_responder responder(blockchain, transactions);

    blockchain.start();
    transactions.start();
    threads.stop();
    threads.join();

    // uninitchain(prefix);
}

BOOST_AUTO_TEST_CASE(node_test__construct_poller__does_not_throw)
{
    // WARNING: file system side effect, use unique relative path.
    const static auto prefix = "node_test/construct_poller";
    initchain(prefix);

    bc::threadpool threads;
    bc::chain::blockchain_impl blockchain(threads, prefix);
    bc::node::poller poller(threads, blockchain);

    blockchain.start();
    blockchain.stop();
    threads.stop();
    threads.join();

    // uninitchain(prefix);
}

BOOST_AUTO_TEST_CASE(node_test__construct_transaction_indexer__does_not_throw)
{
    bc::threadpool threads;
    bc::node::transaction_indexer indexer(threads);

    threads.stop();
    threads.join();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(slow_node_tests)

BOOST_AUTO_TEST_CASE(node_test__construct_session__does_not_throw)
{
    // WARNING: file system side effect, use unique relative path.
    const static auto prefix = "node_test/construct_session";
    initchain(prefix);

    bc::threadpool threads;
    bc::network::hosts hosts(threads);
    bc::network::network network(threads);
    bc::network::handshake handshake(threads);
    bc::network::protocol protocol(threads, hosts, handshake, network);
    bc::chain::blockchain_impl blockchain(threads, prefix);
    bc::chain::transaction_pool transactions(threads, blockchain);
    bc::node::poller poller(threads, blockchain);

    const auto parameters = bc::node::session_params
    {
        handshake, protocol, blockchain, poller, transactions
    };

    const auto noop_handler = [](const std::error_code& ec)
    {
    };

    bc::node::session session(threads, parameters);

    blockchain.start();
    transactions.start();
    session.start(noop_handler);
    session.stop(noop_handler);
    blockchain.stop();
    threads.stop();
    threads.join();

    // uninitchain(prefix);
}

BOOST_AUTO_TEST_SUITE_END()
