/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
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

using namespace bc;
using namespace bc::blockchain;
using namespace bc::network;
using namespace bc::node;
using namespace boost;

struct low_thread_priority_fixture
{
    low_thread_priority_fixture()
    {
        set_thread_priority(thread_priority::lowest);
    }

    ~low_thread_priority_fixture()
    {
        set_thread_priority(thread_priority::normal);
    }
};

static void uninitchain(filesystem::path& path)
{
    boost::filesystem::remove_all(path);
}

static void initchain(filesystem::path& path)
{
    uninitchain(path);
    boost::filesystem::create_directories(path);
    database::initialize(path, mainnet_genesis_block());
}

// TODO: move construction expressions into BOOST_REQUIRE_NO_THROW.
BOOST_FIXTURE_TEST_SUITE(thread_tests, low_thread_priority_fixture)

// Just a basic test to get some coverage output.
BOOST_AUTO_TEST_CASE(node_test__construct_transaction_indexer__does_not_throw)
{
    threadpool threads;
    indexer index(threads);
    threads.shutdown();
    threads.join();
}

BOOST_AUTO_TEST_SUITE_END()

// TODO: move construction expressions into BOOST_REQUIRE_NO_THROW.
BOOST_FIXTURE_TEST_SUITE(node_tests, low_thread_priority_fixture)

BOOST_AUTO_TEST_CASE(node_test__construct_responder__does_not_throw)
{
    // WARNING: file system side effect, use unique relative path.
    configuration config;
    config.chain.database_path = "node_test/construct_responder";
    initchain(config.chain.database_path);

    threadpool threads;
    blockchain_impl blockchain(threads, config.chain);
    transaction_pool transactions(threads, blockchain, 42);
    responder responder(blockchain, transactions);

    // TODO: handle blockchain start.
    blockchain.start([](code){});
    transactions.start();
    blockchain.stop();
    threads.shutdown();
    threads.join();

    ////uninitchain(prefix);
}

BOOST_AUTO_TEST_CASE(node_test__construct_poller__does_not_throw)
{
    // WARNING: file system side effect, use unique relative path.
    configuration config;
    config.chain.database_path = "node_test/construct_poller";
    initchain(config.chain.database_path);

    threadpool threads;
    blockchain_impl blockchain(threads, config.chain);
    poller poller(threads, blockchain);

    // TODO: handle blockchain start.
    blockchain.start([](code){});
    blockchain.stop();
    threads.shutdown();
    threads.join();

    ////uninitchain(prefix);
}

BOOST_AUTO_TEST_SUITE_END()
