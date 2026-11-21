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
#include "../test.hpp"
#include "../functional/p2p_setup_fixture.hpp"
#include <future>

using namespace system;

static constexpr uint64_t parent_value = 100'000;

// A tx with an anyone-can-spend output.
static const chain::transaction& parent() NOEXCEPT
{
    const chain::point point{ one_hash, 0u };
    const chain::operations ops{ { chain::opcode::push_positive_1 } };
    const chain::script spendable{ ops };
    static const chain::transaction instance
    {
        1u,
        chain::inputs{ { point, chain::script{}, max_uint32 } },
        chain::outputs{ { parent_value, spendable } },
        0u
    };

    return instance;
}

static chain::transaction::cptr spend(const hash_digest& hash,
    uint64_t value) NOEXCEPT
{
    const chain::point point{ hash, 0u };
    const chain::operations ops{ { chain::opcode::push_positive_1 } };
    const chain::script spendable{ ops };
    return std::make_shared<const chain::transaction>(chain::transaction
    {
        1u,
        chain::inputs{ { point, chain::script{}, max_uint32 } },
        chain::outputs{ { value, spendable } },
        0u
    });
}

static chain::transaction::cptr spend(uint64_t value) NOEXCEPT
{
    return spend(parent().hash(false), value);
}

static chain::transactions_cptr package(
    const chain::transaction_cptrs& txs) NOEXCEPT
{
    return std::make_shared<const chain::transaction_cptrs>(txs);
}

// Block 1 of a coinbase and the parent.
static const chain::block& parent_block() NOEXCEPT
{
    const system::settings bitcoin{ chain::selection::mainnet };
    const auto& genesis = bitcoin.genesis_block.header();
    const chain::input null{ chain::point{}, chain::script{}, max_uint32 };
    const chain::output out{ 42u, chain::script{} };
    const chain::transaction coinbase
    {
        1u, chain::inputs{ null }, chain::outputs{ out }, 1u
    };

    const auto timestamp = add1(genesis.timestamp());
    const chain::header header
    {
        1u, genesis.hash(), null_hash, timestamp, genesis.bits(), 1u
    };

    static const chain::block instance{ header, { coinbase, parent() } };
    return instance;
}

// The parent confirmed in block 1.
static bool stored_parent(node::query& query) NOEXCEPT
{
    const auto& block = parent_block();
    return query.set(block, database::context{ 0, 1, 0 }, {}, false, true) &&
        query.push_confirmed(query.to_header(block.hash()), true);
}

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

struct chaser_template_setup_fixture
  : p2p_setup_fixture
{
    using built = std::tuple<code, size_t, database::tx_links>;

    inline chaser_template_setup_fixture()
      : p2p_setup_fixture(stored_parent, [](configuration& config)
        {
            using deployment = chain::chain_state::deployment;
            config.network.enable_relay = true;
            config.node.currency_window_minutes = 0;
            config.bitcoin.bip9_bit0_deployment.start = deployment::always_active;
        })
    {
        subscribe();
    }

    code submit(const chain::transactions_cptr& txs)
    {
        std::promise<code> promise{};
        node_.submit(txs, false, [&](const code& ec, size_t) NOEXCEPT
        {
            promise.set_value(ec);
        });

        return promise.get_future().get();
    }

    built get_template()
    {
        std::promise<built> promise{};
        node_.get_template([&](const code& ec, size_t height,
            const database::tx_links& links) NOEXCEPT
        {
            promise.set_value({ ec, height, links });
        });

        return promise.get_future().get();
    }

    // The links of a template built upon request.
    database::tx_links requested()
    {
        return std::get<2>(get_template());
    }

    // The height of the template event of a request (bounded).
    size_t announced()
    {
        get_template();
        node::chases::template_ event{};
        return await(event) ? event.height : max_size_t;
    }

    database::tx_link link(const chain::transaction::cptr& tx)
    {
        return query_.to_tx(tx->hash(false));
    }
};

BC_FIXTURE_TEST_SUITE(chaser_template_tests, chaser_template_setup_fixture)

BC_AUTO_TEST_CASE(chaser_template__get_template__started__empty)
{
    const auto [ec, height, links] = get_template();
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE(links.empty());
}

BC_AUTO_TEST_CASE(chaser_template__get_template__request__template_event_at_next_height)
{
    BC_REQUIRE_EQUAL(announced(), 2u);
}

BC_AUTO_TEST_CASE(chaser_template__get_template__single__templated_at_next_height)
{
    const auto tx = spend(parent_value);
    const auto ec = submit(package({ tx }));
    const auto [built, height, links] = get_template();
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE_EQUAL(built, node::error::success);
    BC_REQUIRE_EQUAL(height, 2u);
    BC_REQUIRE(links == database::tx_links{ link(tx) });
}

BC_AUTO_TEST_CASE(chaser_template__get_template__parent_and_child__parent_first)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto ec = submit(package({ first, second }));
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE(requested() == database::tx_links({ link(first), link(second) }));
}

BC_AUTO_TEST_CASE(chaser_template__get_template__separate_packages__both_templated)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto first_ec = submit(package({ first }));
    const auto second_ec = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(requested() == database::tx_links({ link(first), link(second) }));
}

BC_AUTO_TEST_CASE(chaser_template__get_template__conflict__higher_fee_only)
{
    const auto first = spend(parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_ec = submit(package({ first }));
    const auto second_ec = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(requested() == database::tx_links{ link(second) });
}

BC_AUTO_TEST_CASE(chaser_template__get_template__child_of_losing_rival__rival_only)
{
    const auto first = spend(parent_value);
    const auto child = spend(first->hash(false), parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_ec = submit(package({ first, child }));
    const auto second_ec = submit(package({ second }));
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(requested() == database::tx_links{ link(second) });
}

BC_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
