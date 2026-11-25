/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "chaser_transaction_setup_fixture.hpp"

BC_AUTO_TEST_SUITE(chaser_transaction_compact_tests)

// submit_compact

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__invalid_tx_link__integrity_suspended, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto result = submit_compact(package({}), { database::tx_link{ 42 } }, link);
    BC_REQUIRE_EQUAL(result.first, database::error::integrity);
    BC_REQUIRE(node_.suspended());
    BC_REQUIRE(!query_.is_associated(link));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__unstorable_size__block_weight_limit, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    BC_REQUIRE(query_.set(large_transaction()));

    const auto stored = query_.to_tx(large_transaction().hash(false));
    const auto result = submit_compact(package({}), { stored, stored }, link);
    BC_REQUIRE_EQUAL(result.first, system::error::block_weight_limit);
    BC_REQUIRE(!node_.suspended());
    BC_REQUIRE(query_.is_unconfirmable(link));
    BC_REQUIRE(!query_.is_associated(link));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__stored_unpooled_tx__pooled, chaser_transaction_child_header_setup_fixture)
{
    const auto link = query_.to_header(child_header().hash());
    const auto tx = spend(parent_value);
    BC_REQUIRE(query_.set(*tx));

    const auto stored = query_.to_tx(tx->hash(false));
    BC_REQUIRE(!query_.is_pooled(stored));

    const auto result = submit_compact(package({ tx }), { database::tx_link{ 0 }, database::tx_link::terminal }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_associated(link));
    BC_REQUIRE(query_.is_pooled(stored));
    BC_REQUIRE_EQUAL(query_.to_tx(tx->hash(false)), stored);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__unstored_tx__archived_and_pooled, chaser_transaction_child_header_setup_fixture)
{
    const auto link = query_.to_header(child_header().hash());
    const auto tx = spend(parent_value);
    const auto result = submit_compact(package({ tx }), { database::tx_link{ 0 }, database::tx_link::terminal }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_associated(link));
    BC_REQUIRE(query_.is_pooled(query_.to_tx(tx->hash(false))));
}

// The parent is archived, but neither confirmed nor pooled.
BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__stored_spender_of_unpooled_parent__not_pooled, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto tx = spend(parent_value);
    BC_REQUIRE(query_.set(*tx));

    const auto stored = query_.to_tx(tx->hash(false));
    const auto result = submit_compact(package({ tx }), { database::tx_link{ 0 }, database::tx_link::terminal }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_associated(link));
    BC_REQUIRE(!query_.is_pooled(stored));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__unstored_spender_of_unpooled_parent__archived_not_pooled, chaser_transaction_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto tx = spend(parent_value);
    const auto result = submit_compact(package({ tx }), { database::tx_link{ 0 }, database::tx_link::terminal }, link);
    const auto stored = query_.to_tx(tx->hash(false));
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_associated(link));
    BC_REQUIRE(!stored.is_terminal());
    BC_REQUIRE(!query_.is_pooled(stored));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__stored_coinbase__archived_again, chaser_transaction_header_setup_fixture)
{
    const auto& block1 = p2p_compact_setup_fixture::block1();
    const auto link = query_.to_header(block1.hash());
    const auto coinbase = block1.transactions_ptr()->front();
    BC_REQUIRE(query_.set(*coinbase));

    const auto stored = query_.to_tx(coinbase->hash(false));
    const auto result = submit_compact(package({ coinbase }), { database::tx_link::terminal }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(query_.is_associated(link));
    BC_REQUIRE_EQUAL(query_.to_duplicates(coinbase->hash(false)).size(), 2u);
    BC_REQUIRE(query_.to_tx(coinbase->hash(false)) != stored);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__not_pooling__pooling_disabled, chaser_transaction_closed_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto result = submit_compact(package({}), { database::tx_link{ 0 } }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
    BC_REQUIRE(!query_.is_associated(link));
}

// archive_block

BC_FIXTURE_TEST_CASE(chaser_transaction__archive_block__not_pooling__archived_locally, chaser_transaction_closed_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto result = archive_block(link);
    BC_REQUIRE(result.first);
    BC_REQUIRE(!result.second);
    BC_REQUIRE(query_.is_associated(link));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__archive_block__not_pooling_associated__duplicate_block, chaser_transaction_closed_header_setup_fixture)
{
    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    BC_REQUIRE(!archive_block(link).second);

    const auto result = archive_block(link);
    BC_REQUIRE(result.first);
    BC_REQUIRE_EQUAL(result.second, node::error::duplicate_block);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__archive_block__pooling__archived_on_strand, chaser_transaction_header_setup_fixture)
{
    BC_REQUIRE(bumped(1));

    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto result = archive_block(link);
    BC_REQUIRE(!result.first);
    BC_REQUIRE(!result.second);
    BC_REQUIRE(query_.is_associated(link));
}

BC_FIXTURE_TEST_CASE(chaser_transaction__archive_block__pooling_compact_associated__duplicate_block, chaser_transaction_header_setup_fixture)
{
    BC_REQUIRE(bumped(1));

    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    const auto& coinbase = p2p_compact_setup_fixture::block1().transactions_ptr()->front();
    BC_REQUIRE_EQUAL(submit_compact(package({ coinbase }), { database::tx_link::terminal }, link).first, node::error::success);

    const auto result = archive_block(link);
    BC_REQUIRE(!result.first);
    BC_REQUIRE_EQUAL(result.second, node::error::duplicate_block);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__submit_compact__pooling_archived__duplicate_block, chaser_transaction_header_setup_fixture)
{
    BC_REQUIRE(bumped(1));

    const auto link = query_.to_header(p2p_compact_setup_fixture::block1().hash());
    BC_REQUIRE(!archive_block(link).second);

    const auto& coinbase = p2p_compact_setup_fixture::block1().transactions_ptr()->front();
    const auto result = submit_compact(package({ coinbase }), { database::tx_link::terminal }, link);
    BC_REQUIRE_EQUAL(result.first, node::error::duplicate_block);
}

BC_AUTO_TEST_SUITE_END()
