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

// An unconfirmed tx with an anyone-can-spend output.
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

static bool stored_parent(node::query& query) NOEXCEPT
{
    return query.set(parent());
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

    // True when a template event leaves the expected template (bounded).
    bool templated(const database::tx_links& expected)
    {
        node::chases::template_ event{};
        while (await(event))
            if (std::get<2>(get_template()) == expected)
                return true;

        return false;
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

BC_AUTO_TEST_CASE(chaser_template__transaction__single__templated_at_next_height)
{
    const auto tx = spend(parent_value);
    const auto ec = submit(package({ tx }));
    const auto templated_tx = templated({ link(tx) });
    const auto height = std::get<1>(get_template());
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE(templated_tx);
    BC_REQUIRE_EQUAL(height, 1u);
}

BC_AUTO_TEST_CASE(chaser_template__transaction__parent_and_child__parent_first)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto ec = submit(package({ first, second }));
    const auto templated_both = templated({ link(first), link(second) });
    BC_REQUIRE_EQUAL(ec, node::error::success);
    BC_REQUIRE(templated_both);
}

BC_AUTO_TEST_CASE(chaser_template__transaction__separate_packages__both_templated)
{
    const auto first = spend(parent_value);
    const auto second = spend(first->hash(false), parent_value);
    const auto first_ec = submit(package({ first }));
    const auto second_ec = submit(package({ second }));
    const auto templated_both = templated({ link(first), link(second) });
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(templated_both);
}

BC_AUTO_TEST_CASE(chaser_template__transaction__conflict__higher_fee_only)
{
    const auto first = spend(parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_ec = submit(package({ first }));
    const auto second_ec = submit(package({ second }));
    const auto templated_rival = templated({ link(second) });
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(templated_rival);
}

BC_AUTO_TEST_CASE(chaser_template__transaction__child_of_losing_rival__rival_only)
{
    const auto first = spend(parent_value);
    const auto child = spend(first->hash(false), parent_value);
    const auto second = spend(sub1(parent_value));
    const auto first_ec = submit(package({ first, child }));
    const auto second_ec = submit(package({ second }));
    const auto templated_rival = templated({ link(second) });
    BC_REQUIRE_EQUAL(first_ec, node::error::success);
    BC_REQUIRE_EQUAL(second_ec, node::error::success);
    BC_REQUIRE(templated_rival);
}

BC_AUTO_TEST_SUITE_END()

BC_POP_WARNING()
