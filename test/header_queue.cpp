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
#include <cstddef>
#include <memory>
#include <vector>
#include <boost/test/unit_test.hpp>
#include <bitcoin/node.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::config;
using namespace bc::node;
using namespace bc::message;

BOOST_AUTO_TEST_SUITE(header_queue_tests)

static const checkpoint check42{ "4242424242424242424242424242424242424242424242424242424242424242", 42 };
static const checkpoint::list no_checks;
static const checkpoint::list one_check{ check42 };

// Create a headers message of specified size, using specified previous hash.
static headers::ptr message_factory(size_t count, const hash_digest& hash)
{
    auto previous_hash = hash;
    const auto headers = std::make_shared<message::headers>();
    auto& elements = headers->elements;

    for (size_t height = 0; height < count; ++height)
    {
        const header current_header{ 0, previous_hash, {}, 0, 0, 0, 0 };
        elements.push_back(current_header);
        previous_hash = current_header.hash();
    }

    return headers;
}

// Create a headers message of specified size, starting with a genesis header.
static headers::ptr message_factory(size_t count)
{
    return message_factory(count, null_hash);
}

// empty
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__empty__no_checkpoints__true)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(header_queue__empty__one_checkpoint__true)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE(hashes.empty());
}

BOOST_AUTO_TEST_CASE(header_queue__empty_size__initialize__false)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(!hashes.empty());
}

BOOST_AUTO_TEST_CASE(header_queue__empty__initialize_enqueue_1__false)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message_factory(1, check42.hash())));
    BOOST_REQUIRE(!hashes.empty());
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__empty__initialize_dequeue__true)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE(hashes.empty());
}

// size
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__size__no_checkpoints__0)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE_EQUAL(hashes.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__size__one_checkpoint__0)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE_EQUAL(hashes.size(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__size__initialize__1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_queue__size__initialize_enqueue_1__2)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message_factory(1, check42.hash())));
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__size__initialize_dequeue__0)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE_EQUAL(hashes.size(), 0u);
}

// first_height
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__first_height__no_checkpoints__default)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE_EQUAL(hashes.first_height(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__first_height__one_checkpoint__default)
{
    header_queue hashes(one_check);
    BOOST_REQUIRE_EQUAL(hashes.first_height(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__first_height__initialize__initial)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE_EQUAL(hashes.first_height(), check42.height());
}

BOOST_AUTO_TEST_CASE(header_queue__first_height__initialize_enqueue_1__initial)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message_factory(1, check42.hash())));
    BOOST_REQUIRE_EQUAL(hashes.first_height(), check42.height());
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__first_height__initialize_dequeue__initial_plus_1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE_EQUAL(hashes.first_height(), check42.height() + 1);
}

// last_height
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__flast_height__no_checkpoints__default)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE_EQUAL(hashes.last_height(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__last_height__one_checkpoint__default)
{
    header_queue hashes(one_check);
    BOOST_REQUIRE_EQUAL(hashes.last_height(), 0u);
}

BOOST_AUTO_TEST_CASE(header_queue__last_height__initialize__initial)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE_EQUAL(hashes.last_height(), check42.height());
}

BOOST_AUTO_TEST_CASE(header_queue__last_height__initialize_enqueue_1__initial_plus_1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message_factory(1, check42.hash())));
    BOOST_REQUIRE_EQUAL(hashes.last_height(), check42.height() + 1);
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__last_height__initialize_dequeue__initial_plus_1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE_EQUAL(hashes.last_height(), check42.height() + 1);
}

// last_hash
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__last_hash__no_checkpoints__null_hash)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE(hashes.last_hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(header_queue__last_hash__one_checkpoint__null_hash)
{
    header_queue hashes(one_check);
    BOOST_REQUIRE(hashes.last_hash() == null_hash);
}

BOOST_AUTO_TEST_CASE(header_queue__last_hash__initialize__expected)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.last_hash() == check42.hash());
}

BOOST_AUTO_TEST_CASE(header_queue__last_hash__initialize_enqueue_1__expected)
{
    const auto message = message_factory(1, check42.hash());
    const auto expected = message->elements[0].hash();

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE(hashes.last_hash() == expected);
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__last_hash__initialize_dequeue__null_hash)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE(hashes.last_hash() == null_hash);
}

// dequeue1
//-----------------------------------------------------------------------------

// This isa dead corner case just to satisfy the parameter domain.
BOOST_AUTO_TEST_CASE(header_queue__dequeue1__empty_dequeue_0__true)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE(hashes.dequeue(0));
}

BOOST_AUTO_TEST_CASE(header_queue__dequeue1__empty_dequeue_1__false)
{
    header_queue hashes(no_checks);
    BOOST_REQUIRE(!hashes.dequeue(1));
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__dequeue1__enqueue_1_dequeue_1__true_empty)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.dequeue(1));
    BOOST_REQUIRE(hashes.empty());
}

// The chain is broken when the list is emptied.
BOOST_AUTO_TEST_CASE(header_queue__dequeue1__size_exceeded__false_empty)
{
    const auto message = message_factory(3, check42.hash());

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE(!hashes.dequeue(5));
    BOOST_REQUIRE(hashes.empty());
}

// dequeue2
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__dequeue2__empty__false)
{
    header_queue hashes(no_checks);
    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(!hashes.dequeue(hash, height));
}

BOOST_AUTO_TEST_CASE(header_queue__dequeue2__initialize__true_expected)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == check42.hash());
    BOOST_REQUIRE_EQUAL(height, check42.height());
}

BOOST_AUTO_TEST_CASE(header_queue__dequeue2__initialize_enqueue_1__true_expected)
{
    const auto message = message_factory(1, check42.hash());

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == check42.hash());
    BOOST_REQUIRE_EQUAL(height, check42.height());
}

BOOST_AUTO_TEST_CASE(header_queue__dequeue2__initialize_enqueue_1_dequeue__true_expected)
{
    const auto message = message_factory(1, check42.hash());
    const auto expected = message->elements[0].hash();

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE(hashes.dequeue());
    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == expected);
    BOOST_REQUIRE_EQUAL(height, check42.height() + 1);
}

// enqueue
//-----------------------------------------------------------------------------

// Can never merge to an empty chain, as it wouldn't be chained.
BOOST_AUTO_TEST_CASE(header_queue__enqueue__enqueue_1__false_empty)
{
    const auto message = message_factory(1);

    header_queue hashes(no_checks);
    BOOST_REQUIRE(!hashes.enqueue(message));
    BOOST_REQUIRE(hashes.empty());
}

// Merging an empty message is okay, as long as there is a non-empty queue.
BOOST_AUTO_TEST_CASE(header_queue__enqueue__initialize_enqueue_0__true_size_1)
{
    const auto message = message_factory(0);

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

// This tests capacity excess with no head offset.
BOOST_AUTO_TEST_CASE(header_queue__enqueue__initialize_enqueue_1__size_2)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message_factory(1, check42.hash())));
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
}

// The chain is broken when the list is emptied.
// This tests capacity excess with a head offset.
BOOST_AUTO_TEST_CASE(header_queue__enqueue__initialize_enqueue_1_dequeue_enqueue_2_dequeue__expected)
{
    const auto message1 = message_factory(1, check42.hash());
    const auto expected = message1->elements[0].hash();
    const auto message2 = message_factory(2, expected);

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message1));
    BOOST_REQUIRE(hashes.dequeue());
    BOOST_REQUIRE(hashes.enqueue(message2));
    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == expected);
    BOOST_REQUIRE_EQUAL(height, check42.height() + 1);
    BOOST_REQUIRE_EQUAL(hashes.size(), 2u);
}

BOOST_AUTO_TEST_CASE(header_queue__enqueue__linked__true_expected_order)
{
    const auto message = message_factory(2, check42.hash());

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.size(), 3u);

    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == check42.hash());
    BOOST_REQUIRE_EQUAL(height, check42.height());
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == message->elements[0].hash());
    BOOST_REQUIRE_EQUAL(height, check42.height() + 1);
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == message->elements[1].hash());
    BOOST_REQUIRE_EQUAL(height, check42.height() + 2);
}

BOOST_AUTO_TEST_CASE(header_queue__enqueue__unlinked__false_expected)
{
    // This is unlined to the seed hash so must cause a linkage failure.
    const auto message = message_factory(1 /*, check42.hash() */);

    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE(!hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_queue__enqueue__checkpoint_match__true_expected)
{
    const auto message = message_factory(2, check42.hash());
    const checkpoint::list checkpoints
    {
        { message->elements[1].hash(), check42.height() + 2 }
    };

    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    BOOST_REQUIRE(hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.size(), 3u);

    size_t height;
    hash_digest hash;
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == check42.hash());
    BOOST_REQUIRE_EQUAL(height, check42.height());
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == message->elements[0].hash());
    BOOST_REQUIRE_EQUAL(height, check42.height() + 1);
    BOOST_REQUIRE(hashes.dequeue(hash, height));
    BOOST_REQUIRE(hash == message->elements[1].hash());
    BOOST_REQUIRE_EQUAL(height, check42.height() + 2);
}

BOOST_AUTO_TEST_CASE(header_queue__enqueue__single_checkpoint_mismatch__false_rollback_to_initial)
{
    const auto message = message_factory(5, check42.hash());

    // The hash at this height will not match so must cause checkpont failure.
    const checkpoint::list checkpoints
    {
        { /*message->elements[1].hash()*/ null_hash, check42.height() + 5 }
    };

    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    BOOST_REQUIRE(!hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_queue__enqueue__multiple_checkpoint_mismatch__false_rollback_to_preceding_checkpoint)
{
    const auto message = message_factory(9, check42.hash());

    const auto expected_height = check42.height() + 3;
    const auto expected_hash = message->elements[2].hash();
    const auto expected_size = expected_height - check42.height() + 1;

    // The hash at the intermediate height will not match so must cause checkpont failure.
    const checkpoint::list checkpoints
    {
        { expected_hash, expected_height },
        { /*message->elements[1].hash()*/ null_hash, check42.height() + 5 },
        { message->elements[9].hash(), check42.height() + 10 },
    };

    header_queue hashes(checkpoints);
    hashes.initialize(check42);
    BOOST_REQUIRE(!hashes.enqueue(message));
    BOOST_REQUIRE_EQUAL(hashes.last_height(), expected_height);
    BOOST_REQUIRE(hashes.last_hash() == expected_hash);
    BOOST_REQUIRE_EQUAL(hashes.size(), expected_size);
}

// initialize
//-----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(header_queue__initialize1__always__size_1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42);
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(header_queue__initialize2__always__size_1)
{
    header_queue hashes(no_checks);
    hashes.initialize(check42.hash(), check42.height());
    BOOST_REQUIRE_EQUAL(hashes.size(), 1u);
}

BOOST_AUTO_TEST_SUITE_END()
