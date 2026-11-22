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

namespace libbitcoin {
namespace node {

#define CLASS chaser_transaction

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// relay slots
// ----------------------------------------------------------------------------

// A tier takes the lowest free slot outside the reserves of the tiers above.
size_t chaser_transaction::reserve_relay(relay_tier tier) NOEXCEPT
{
    uint64_t excluded{};
    if (tier == relay_tier::outbound)
        excluded = outbound_excluded_;
    else if (tier == relay_tier::inbound)
        excluded = inbound_excluded_;

    auto reserved = reserved_.load();
    auto blocked = bit_or(reserved, excluded);
    while (blocked != bit_all<uint64_t>)
    {
        const auto slot = right_ones(blocked);
        if (reserved_.compare_exchange_weak(reserved,
            set_right(reserved, slot)))
            return slot;

        blocked = bit_or(reserved, excluded);
    }

    return relay_slots;
}

void chaser_transaction::activate_relay(size_t slot, object_key key) NOEXCEPT
{
    if (closed() || slot >= relay_slots)
        return;

    POST(do_activate_relay, slot, key);
}

void chaser_transaction::release_relay(size_t slot) NOEXCEPT
{
    if (closed() || slot >= relay_slots)
        return;

    POST(do_release_relay, slot);
}

void chaser_transaction::set_relay_rate(size_t slot, uint64_t rate) NOEXCEPT
{
    if (closed() || slot >= relay_slots)
        return;

    POST(do_set_relay_rate, slot, rate);
}

void chaser_transaction::set_relayed(size_t slot,
    const database::tx_links& links) NOEXCEPT
{
    if (closed() || slot >= relay_slots)
        return;

    POST(do_set_relayed, slot, links);
}

void chaser_transaction::relay_pool(size_t slot) NOEXCEPT
{
    if (closed() || slot >= relay_slots)
        return;

    POST(do_relay_pool, slot);
}

// Slot is relayed to once its channel is notified, else marks precede sends.
void chaser_transaction::do_activate_relay(size_t slot,
    object_key key) NOEXCEPT
{
    BC_ASSERT(stranded());
    keys_.at(slot) = key;
    set_right_into(relaying_, slot);
    if (!sweeping_)
        do_sweep_relay();
}

void chaser_transaction::do_release_relay(size_t slot) NOEXCEPT
{
    BC_ASSERT(stranded());
    for (auto& mark: marks_)
        set_right_into(mark, slot, false);

    rates_.at(slot) = zero;
    keys_.at(slot) = {};
    set_right_into(relaying_, slot, false);
    set_right_into(owed_, slot, false);
    set_right_into(swept_, slot, false);
    reserved_.fetch_and(bit_not(bit_right<uint64_t>(slot)));
}

// A rise leaves the slot's marks, a fall relays to it what now clears.
void chaser_transaction::do_set_relay_rate(size_t slot, uint64_t rate) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto prior = rates_.at(slot);
    rates_.at(slot) = rate;
    if (rate < prior)
        owe_relay(slot);
}

void chaser_transaction::do_relay_pool(size_t slot) NOEXCEPT
{
    BC_ASSERT(stranded());
    owe_relay(slot);
}

// private
// Sweep is a pool scan at peer request, so one per slot between blocks.
void chaser_transaction::owe_relay(size_t slot) NOEXCEPT
{
    BC_ASSERT(stranded());
    set_right_into(owed_, slot);
    if (!sweeping_)
        do_sweep_relay();
}

// private
uint64_t chaser_transaction::sweepable() const NOEXCEPT
{
    return bit_and(bit_and(owed_, relaying_), bit_not(swept_));
}

// One slot swept per strand turn, so sweeps do not hold up other work.
void chaser_transaction::do_sweep_relay() NOEXCEPT
{
    BC_ASSERT(stranded());
    sweeping_ = false;
    if (closed() || !pooling_ || is_zero(sweepable()))
        return;

    const auto slot = right_zeros(sweepable());
    set_right_into(owed_, slot, false);
    set_right_into(swept_, slot);
    if (!sweep_relay(slot))
    {
        fault(database::error::integrity);
        return;
    }

    if (is_zero(sweepable()))
        return;

    sweeping_ = true;
    POST(do_sweep_relay);
}

// private
bool chaser_transaction::sweep_relay(size_t slot) NOEXCEPT
{
    database::tx_links links{};
    const auto& query = archive();
    if (query.get_confirmable_txs(links))
        return false;

    const auto word = bit_right<uint64_t>(slot);
    for (const auto& link: links)
    {
        const auto row = query.to_pool_row(link);
        if (row.is_terminal() || get_right(marks(row.value), slot))
            continue;

        if (!relay({ link }, relay_slots, word, keys_.at(slot)))
            return false;
    }

    return true;
}

void chaser_transaction::do_set_relayed(size_t slot,
    const database::tx_links& links) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    for (const auto& link: links)
    {
        const auto row = query.to_pool_row(link);
        if (!row.is_terminal())
            set_right_into(marks(row.value), slot);
    }
}

// private
// Relay events go to every channel, or with a key only to its own.
bool chaser_transaction::relay(const database::tx_links& links,
    size_t origin, uint64_t slots, object_key key) NOEXCEPT
{
    BC_ASSERT(stranded());
    using algorithm = pool::algorithm;
    struct relayable
    {
        size_t row;
        database::tx_link link;
        algorithm::totals_t totals;
    };

    const auto& query = archive();
    std::vector<relayable> rows{};
    std::unordered_set<database::tx_link::integer> seen{};
    const auto add = [&](const database::tx_link& link) NOEXCEPT
    {
        if (!seen.insert(link.value).second)
            return true;

        algorithm::totals_t totals{};
        const auto row = query.to_pool_row(link);
        if (row.is_terminal() || !query.get_pooled_totals(totals, link))
            return false;

        rows.push_back({ row.value, link, totals });
        return true;
    };

    for (const auto& link: links)
    {
        for (const auto& ancestor: query.to_confirmable_ancestors(link))
            if (!add(ancestor))
                return false;

        if (!add(link))
            return false;
    }

    std::ranges::sort(rows, {}, &relayable::row);

    const auto limit = block_limit().size;
    const auto bytes_per_sigop = node_settings().pool.bytes_per_sigop;
    std::array<uint64_t, relay_slots> fees{};
    std::array<uint64_t, relay_slots> sizes{};
    for (const auto& row: rows)
    {
        auto& mark = marks(row.row);
        if (origin < relay_slots)
            set_right_into(mark, origin);

        const auto lacking = bit_and(slots, bit_not(mark));
        const auto size = algorithm::chargeable(row.totals, bytes_per_sigop);
        for (size_t slot{}; slot < relay_slots; ++slot)
        {
            if (get_right(lacking, slot))
            {
                fees.at(slot) = ceilinged_add(fees.at(slot), row.totals.fee);
                sizes.at(slot) = ceilinged_add(sizes.at(slot), size);
            }
        }
    }

    // Compared in satoshis per virtual kilobyte.
    uint64_t clears{};
    for (size_t slot{}; slot < relay_slots; ++slot)
    {
        const auto size = sizes.at(slot);
        const auto paid = ceilinged_multiply(fees.at(slot), 1'000_u64);
        const auto owed = ceilinged_multiply(rates_.at(slot), size);
        if (!is_zero(size) && size <= limit && paid >= owed)
            set_right_into(clears, slot);
    }

    for (const auto& row: rows)
    {
        auto& mark = marks(row.row);
        const auto word = bit_and(clears, bit_not(mark));
        if (is_zero(word))
            continue;

        mark = bit_or(mark, word);
        const chases::relay event{ row.link.value, word };
        if (is_zero(key))
            notify(error::success, event);
        else
            notify_one(key, error::success, event);
    }

    return true;
}

// private
uint64_t& chaser_transaction::marks(size_t row) NOEXCEPT
{
    if (row >= marks_.size())
        marks_.resize(add1(row));

    return marks_.at(row);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
