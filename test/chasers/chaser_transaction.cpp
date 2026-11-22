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

BC_AUTO_TEST_SUITE(chaser_transaction_tests)

// do_bump

BC_FIXTURE_TEST_CASE(chaser_transaction__organized__invalid_confirmed_top__suspended_pooling_disabled, chaser_transaction_pooling_setup_fixture)
{
    BC_REQUIRE(query_.push_confirmed(database::header_link{ 42 }, false));
    node_.notify(node::error::success, chases::organized{ node::header_t{ 42 } });
    BC_REQUIRE(suspended());
    BC_REQUIRE_EQUAL(submit(package({ spend(parent_value) })).first, node::error::pooling_disabled);
}

// subscribe_pool

BC_FIXTURE_TEST_CASE(chaser_transaction__subscribe_pool__submit__coverage_then_admission, chaser_transaction_single_setup_fixture)
{
    const auto events = reported(2, [&]()
    {
        submit(package({ spend(parent_value - 1'000) }));
    });

    BC_REQUIRE_EQUAL(events.size(), 2u);
    BC_REQUIRE(events.front().scan == node::pool_scan::coverage);
    BC_REQUIRE(events.back().scan == node::pool_scan::admission);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__subscribe_pool__get_confirmable_fees__confirmable_reported, chaser_transaction_single_setup_fixture)
{
    const auto events = reported(1, [&]()
    {
        get_confirmable_fees();
    });

    BC_REQUIRE_EQUAL(events.size(), 1u);
    BC_REQUIRE(events.front().scan == node::pool_scan::confirmable);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__subscribe_pool__build_template__template_reported, chaser_transaction_single_setup_fixture)
{
    const auto events = reported(1, [&]()
    {
        build_template();
    });

    BC_REQUIRE_EQUAL(events.size(), 1u);
    BC_REQUIRE(events.front().scan == node::pool_scan::template_);
    BC_REQUIRE_EQUAL(events.front().report.passes, 1u);
    BC_REQUIRE(!events.front().report.device);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__subscribe_pool__reorganized__block_out_then_context, chaser_transaction_single_setup_fixture)
{
    const auto block = confirm();
    const auto raised = bumped(3);
    const auto popped = query_.pop_confirmed();
    const auto events = reported(2, [&]()
    {
        node_.notify(node::error::success, chases::reorganized{ block });
    });

    BC_REQUIRE(!block.is_terminal());
    BC_REQUIRE(raised);
    BC_REQUIRE(popped);
    BC_REQUIRE_EQUAL(events.size(), 2u);
    BC_REQUIRE(events.front().scan == node::pool_scan::block_out);
    BC_REQUIRE(events.back().scan == node::pool_scan::context);
}

// do_bump

BC_FIXTURE_TEST_CASE(chaser_transaction__submit__context_without_bip113__pooling_disabled, chaser_transaction_inactive_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    BC_REQUIRE_EQUAL(result.first, node::error::pooling_disabled);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reorganized__row_of_later_height__insufficient_until_regained, chaser_transaction_single_setup_fixture)
{
    constexpr auto insufficient = bit_or(pool::algorithm::absent, pool::algorithm::insufficient);
    const auto block = confirm();
    const auto raised = bumped(3);
    const auto tx = spend(parent_value - 1'000);
    const auto result = submit(package({ tx }));
    const auto link = query_.to_tx(tx->hash(false));
    const auto before = cell(link);
    const auto popped = query_.pop_confirmed();
    node_.notify(node::error::success, chases::reorganized{ block });
    const auto marked = celled(link, insufficient);
    const auto pushed = query_.push_confirmed(block, true);
    node_.notify(node::error::success, chases::organized{ block });
    const auto restored = celled(link, pool::algorithm::unlabeled);

    BC_REQUIRE(!block.is_terminal());
    BC_REQUIRE(raised);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(pool::algorithm::is_present(before));
    BC_REQUIRE(popped);
    BC_REQUIRE(marked);
    BC_REQUIRE(pushed);
    BC_REQUIRE(restored);
}

// do_organized

BC_FIXTURE_TEST_CASE(chaser_transaction__organized__pooled_block_tx__boundary, chaser_transaction_single_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto result = submit(package({ tx }));
    const auto link = query_.to_tx(tx->hash(false));
    const auto before = cell(link);
    const auto confirmed = !confirm(tx).is_terminal();
    const auto bounded = celled(link, pool::algorithm::boundary);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(pool::algorithm::is_present(before));
    BC_REQUIRE(confirmed);
    BC_REQUIRE(bounded);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__organized__relay_off_pooled_block_tx__index_unchanged, chaser_transaction_relay_off_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto link = query_.to_tx(tx->hash(false));
    const auto before = cell(link);

    // The subscription completes after the chaser is notified of the block.
    auto confirmed = false;
    const auto spans = fired(events::pool_block_in_usecs, [&]()
    {
        confirmed = !confirm(tx).is_terminal();
        subscribe();
    });

    BC_REQUIRE(query_.is_pooling());
    BC_REQUIRE(pool::algorithm::is_present(before));
    BC_REQUIRE(confirmed);
    BC_REQUIRE(spans.empty());
    BC_REQUIRE_EQUAL(cell(link), before);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__organized__pooled_tx__pool_block_in_fired, chaser_transaction_single_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    const auto spans = fired(events::pool_block_in_usecs, [&]()
    {
        reported(1, [&]() { confirm(); });
    });

    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(spans.size(), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__organized__pooled_tx__pool_rows_fired, chaser_transaction_single_setup_fixture)
{
    const auto result = submit(package({ spend(parent_value - 1'000) }));
    const auto rows = fired(events::pool_rows, [&]()
    {
        reported(1, [&]() { confirm(); });
    });

    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE_EQUAL(rows.size(), 1u);
    BC_REQUIRE_EQUAL(rows.front(), 1u);
}

// The subscription completes after the chaser is notified of the block.
BC_FIXTURE_TEST_CASE(chaser_transaction__organized__relay_off__pool_rows_not_fired, chaser_transaction_relay_off_setup_fixture)
{
    const auto rows = fired(events::pool_rows, [&]()
    {
        confirm();
        subscribe();
    });

    BC_REQUIRE(query_.is_pooling());
    BC_REQUIRE(rows.empty());
}

// do_reorganized

BC_FIXTURE_TEST_CASE(chaser_transaction__reorganized__pooled_block_tx__present, chaser_transaction_single_setup_fixture)
{
    const auto tx = spend(parent_value - 1'000);
    const auto result = submit(package({ tx }));
    const auto link = query_.to_tx(tx->hash(false));
    const auto before = cell(link);
    const auto block = confirm(tx);
    const auto bounded = celled(link, pool::algorithm::boundary);
    const auto popped = query_.pop_confirmed();
    node_.notify(node::error::success, chases::reorganized{ block });

    const auto restored = celled(link, before);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(pool::algorithm::is_present(before));
    BC_REQUIRE(!block.is_terminal());
    BC_REQUIRE(bounded);
    BC_REQUIRE(popped);
    BC_REQUIRE(restored);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reorganized__unpooled_block_tx__pooled, chaser_transaction_reorganized_setup_fixture)
{
    const auto& block = reorganized_block();
    const auto& txs = *block.transactions_ptr();
    const auto link = query_.to_header(block.hash());
    const auto stored = query_.to_tx(txs.at(1)->hash(false));
    BC_REQUIRE(!query_.is_pooled(stored));

    node_.notify(node::error::success, chases::reorganized{ link });
    BC_REQUIRE(pooled(stored));
    BC_REQUIRE(!query_.is_pooled(query_.to_tx(txs.front()->hash(false))));

    database::table::pool::record record{};
    BC_REQUIRE(store_.pool.get(store_.pool.first(stored), record));
    BC_REQUIRE_EQUAL(record.time, block.header().timestamp());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__reorganized__relative_spender_of_reconfirmed_parent__revalidated, chaser_transaction_relative_setup_fixture)
{
    constexpr auto insufficient = bit_or(pool::algorithm::absent, pool::algorithm::insufficient);
    const auto block2 = confirm();
    const auto raised = bumped(3);
    const auto tx = spend_relative(parent().hash(false), parent_value - 1'000, 2);
    const auto result = submit(package({ tx }));
    const auto link = query_.to_tx(tx->hash(false));
    const auto before = cell(link);
    const auto popped2 = query_.pop_confirmed();
    node_.notify(node::error::success, chases::reorganized{ block2 });
    const auto lowered2 = bumped(2);
    const auto block1 = query_.to_header(parent_block().hash());
    const auto popped1 = query_.pop_confirmed();
    node_.notify(node::error::success, chases::reorganized{ block1 });
    const auto lowered1 = bumped(1);
    const auto other1 = confirm();
    const auto other2 = confirm(to_shared<chain::transaction>(parent()));
    const auto regained = bumped(3);
    const auto locked = cell(link);
    const auto other3 = confirm();
    const auto unlocked = celled(link, pool::algorithm::unlabeled);

    BC_REQUIRE(!block2.is_terminal());
    BC_REQUIRE(raised);
    BC_REQUIRE_EQUAL(result.first, node::error::success);
    BC_REQUIRE(pool::algorithm::is_present(before));
    BC_REQUIRE(popped2);
    BC_REQUIRE(lowered2);
    BC_REQUIRE(popped1);
    BC_REQUIRE(lowered1);
    BC_REQUIRE(!other1.is_terminal());
    BC_REQUIRE(!other2.is_terminal());
    BC_REQUIRE(regained);
    BC_REQUIRE_EQUAL(locked, insufficient);
    BC_REQUIRE(!other3.is_terminal());
    BC_REQUIRE(unlocked);
}

BC_AUTO_TEST_SUITE_END()

BC_AUTO_TEST_SUITE(chaser_transaction_scan_tests)

// BIP352 send_and_receive_test_vectors.json: "Simple send: two inputs".
static chain::input to_input(const hash_digest& hash, const data_chunk& script,
    const data_chunk& prevout) NOEXCEPT
{
    chain::input in{ { hash, 0 }, { script, false }, {}, max_uint32 };
    const chain::script prevout_script{ prevout, false };
    in.prevout = to_shared<chain::output>(0u, prevout_script);
    return in;
}

static const chain::transaction simple_send
{
    2u,
    chain::inputs
    {
        to_input
        (
            base16_hash("f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16"),
            base16_chunk("483046022100ad79e6801dd9a8727f342f31c71c4912866f59dc6e7981878e92c5844a0ce929022100fb0d2393e813968648b9753b7e9871d90ab3d815ebf91820d704b19f4ed224d621025a1e61f898173040e20616d43e9f496fba90338a39faa1ed98fcbaeee4dd9be5"),
            base16_chunk("76a91419c2f3ae0ca3b642bd3e49598b8da89f50c1416188ac")
        ),
        to_input
        (
            base16_hash("a1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"),
            base16_chunk("48304602210086783ded73e961037e77d49d9deee4edc2b23136e9728d56e4491c80015c3a63022100fda4c0f21ea18de29edbce57f7134d613e044ee150a89e2e64700de2d4e83d4e2103bd85685d03d111699b15d046319febe77f8de5286e9e512703cdee1bf3be3792"),
            base16_chunk("76a914d9317c66f54ff0a152ec50b1d19c25be50c8e15988ac")
        )
    },
    chain::outputs
    {
        { 0u, chain::script{ base16_chunk("51203e9fce73d4e77a4809908e3c3a2e54ee147b9312dc5044a193d1fc85de46e3c1"), false } }
    },
    0u
};

// Mainnet block 1 confirmed (coinbase link 1), scan rows linked 1 and 5.
static bool stored_scan(node::query& query) NOEXCEPT
{
    const auto& block1 = p2p_compact_setup_fixture::block1();
    const database::context context1{ 0, 1, 1231006505 };
    const auto work1 = block1.header().proof() * 2u;
    return query.set(block1, context1, work1, false, false)
        && query.push_confirmed(1, true)
        && query.set_silent(1, simple_send)
        && query.set_silent(5, simple_send);
}

struct chaser_transaction_scan_setup_fixture
  : chaser_transaction_setup_fixture
{
    inline chaser_transaction_scan_setup_fixture(bool relay, bool current,
        const initializer& setup=stored_scan)
      : chaser_transaction_setup_fixture([=](configuration& config)
        {
            config.network.enable_relay = relay;
            config.node.currency_window_minutes = current ? 0 : 1440;
            config.node.silent_start_height = 0;
        }, setup)
    {
    }

    bool mapped(const database::header_link& link)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + seconds(10);
        while (steady_clock::now() < deadline)
        {
            if (!query_.get_scan_start(link).is_terminal())
                return true;

            std::this_thread::sleep_for(milliseconds(10));
        }

        return false;
    }

};

struct scan_current_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_current_fixture()
      : chaser_transaction_scan_setup_fixture(false, true)
    {
    }
};

struct scan_pooling_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_pooling_fixture()
      : chaser_transaction_scan_setup_fixture(true, true)
    {
    }
};

struct scan_stale_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_stale_fixture()
      : chaser_transaction_scan_setup_fixture(false, false)
    {
    }
};

struct scan_complete_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_complete_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query)
                && query.set_scan_start(0, 42)
                && query.set_scan_start(1, 43);
        })
    {
    }
};

struct scan_partial_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_partial_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query) && query.set_scan_start(1, 43);
        })
    {
    }
};

struct scan_unextended_fixture
  : chaser_transaction_scan_setup_fixture
{
    inline scan_unextended_fixture()
      : chaser_transaction_scan_setup_fixture(false, true, [](node::query& query)
        {
            return stored_scan(query) && query.set_scan_start(0, 0);
        })
    {
    }
};

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__current_not_pooling__mapped, scan_current_fixture)
{
    BC_REQUIRE(mapped(0));
    BC_REQUIRE(!query_.is_pooling());
    BC_REQUIRE_EQUAL(query_.get_scan_frontier(), 0u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__current_pooling__mapped_and_latched, scan_pooling_fixture)
{
    BC_REQUIRE(mapped(0));
    BC_REQUIRE(query_.is_pooling());
    BC_REQUIRE_EQUAL(query_.get_scan_frontier(), 2u);
    BC_REQUIRE_EQUAL(query_.get_latch_tx(), 2u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__not_current__unmapped, scan_stale_fixture)
{
    BC_REQUIRE(query_.get_scan_start(0).is_terminal());
    BC_REQUIRE(query_.get_scan_start(1).is_terminal());
}

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__complete_map__unchanged, scan_complete_fixture)
{
    BC_REQUIRE_EQUAL(query_.get_scan_start(0), 42u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(1), 43u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__partial_map__remapped, scan_partial_fixture)
{
    BC_REQUIRE(mapped(0));
    BC_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BC_FIXTURE_TEST_CASE(chaser_transaction__scan__unmapped_top__extended, scan_unextended_fixture)
{
    BC_REQUIRE(mapped(1));
    BC_REQUIRE_EQUAL(query_.get_scan_start(0), 0u);
    BC_REQUIRE_EQUAL(query_.get_scan_start(1), 1u);
}

BC_AUTO_TEST_SUITE_END()
