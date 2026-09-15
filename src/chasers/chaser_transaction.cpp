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
#include <bitcoin/node/chasers/chaser_transaction.hpp>

#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/validate.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_transaction

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

chaser_transaction::chaser_transaction(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start
// ----------------------------------------------------------------------------

code chaser_transaction::start() NOEXCEPT
{
    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    POST(do_bump);
    return error::success;
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_transaction::handle_chase(const code&, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    switch (event_)
    {
        case chase::organized:
        case chase::reorganized:
        {
            POST(do_bump);
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// The pool is closed until the confirmed chain is current, and closes again if
// currency is lost, though the store latch is one way, since the txs archived
// while it was open outlive it.
void chaser_transaction::do_bump() NOEXCEPT
{
    BC_ASSERT(stranded());
    pooling_ = false;

    if (closed() || !is_current_chain(true))
        return;

    auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto state = query.get_confirmed_chain_state(system_settings(),
        query.to_confirmed(top), top);

    if (!state)
    {
        fault(error::transaction1);
        return;
    }

    // The context of the next block, in which a pool tx would confirm.
    pool_ = chain_state{ *state, system_settings() }.context();
    query.set_pooling();
    pooling_ = true;
}

// methods
// ----------------------------------------------------------------------------

void chaser_transaction::submit(const transactions_cptr& txs,
    submit_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_submit, txs, std::move(handler));
}

// private
// The package is accepted as a whole, so nothing is archived until all txs
// have validated, and the index identifies the tx that a failure pertains to.
void chaser_transaction::do_submit(const transactions_cptr& txs,
    const submit_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, zero);
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, zero);
        return;
    }

    if (txs->empty())
    {
        handler(error::empty_package, zero);
        return;
    }

    // Prevouts internal to the package are populated from it, and those that
    // remain are populated from the archive by each tx validation.
    constexpr auto coinbase = false;
    if (const auto ec = block::populate(*txs, pool_, coinbase))
    {
        handler(ec, zero);
        return;
    }

    auto& query = archive();
    for (size_t index{}; index < txs->size(); ++index)
    {
        if (const auto ec = validate_transaction(*txs->at(index), query, pool_))
        {
            handler(ec, index);
            return;
        }
    }

    // A fault here leaves a prefix of the package archived, which the caller
    // resolves by resubmission (an archived tx substitutes its own link).
    for (size_t index{}; index < txs->size(); ++index)
    {
        database::tx_link link{};
        if (const auto ec = query.set_code(link, *txs->at(index)))
        {
            handler(fault(ec), index);
            return;
        }

        fire(events::tx_archived, link);
        notify(error::success, chase::transaction, transaction_t{ link });
    }

    handler(error::success, zero);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
