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

// queries
// ----------------------------------------------------------------------------

// The size arriving in a block at or above the rate.
uint64_t chaser_transaction::arriving(const query::pool_arrivals& arrivals,
    uint64_t rate, estimate_mode mode) NOEXCEPT
{
    using algorithm = pool::algorithm;
    uint64_t size{};

    switch (mode)
    {
        case estimate_mode::geometric:
        {
            for (const auto& takes: std::views::reverse(arrivals))
            {
                const auto above = algorithm::above(takes, rate);
                size = to_half(ceilinged_add(size, above));
            }

            return size;
        }
        case estimate_mode::economical:
        {
            const auto window = economical_estimate_window;
            const auto count = std::min(arrivals.size(), window);
            for (size_t age{}; age < count; ++age)
            {
                const auto above = algorithm::above(arrivals.at(age), rate);
                size = ceilinged_add(size, above);
            }

            return is_zero(count) ? size : floored_divide(size, count);
        }
        case estimate_mode::conservative:
        {
            for (const auto& takes: arrivals)
                size = std::max(size, algorithm::above(takes, rate));

            return size;
        }
        default:
        {
            return size;
        }
    }
}

uint64_t chaser_transaction::pool_generation() const NOEXCEPT
{
    return generation_.load(std::memory_order_relaxed);
}

void chaser_transaction::build_template(template_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_build_template, std::move(handler));
}

void chaser_transaction::estimate(size_t target, estimate_mode mode,
    estimate_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_estimate, target, mode, std::move(handler));
}

void chaser_transaction::get_confirmable_txs(
    confirmable_txs_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_get_confirmable_txs, std::move(handler));
}

void chaser_transaction::get_confirmable_fees(
    confirmable_fees_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_get_confirmable_fees, std::move(handler));
}

// private
void chaser_transaction::do_build_template(
    const template_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {}, {}, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, {}, {}, {});
        return;
    }

    // The template is the first block of a projection.
    const auto& settings = node_settings().pool;
    query::pool_projection projection{};
    database::tx_links links{};
    const auto start = now();
    const auto ec = archive().get_pool_template(links, projection,
        block_limit(), settings.maximum_chunk_depth, settings.cutoff_rate_(),
        settings.bytes_per_sigop, settings.maximum_outcomes);

    report(pool_scan::template_, start);
    handler(ec, context_.pool, context_.previous, links);
}

// private
void chaser_transaction::do_estimate(size_t target, estimate_mode mode,
    const estimate_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (!network_settings().enable_relay)
    {
        handler(error::estimate_disabled, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::estimate_premature, {});
        return;
    }

    if (mode == estimate_mode::unknown || target >= maximum_estimate_horizon)
    {
        handler(error::estimate_false, {});
        return;
    }

    const auto& query = archive();
    const auto& settings = node_settings().pool;
    const auto count = add1(target);
    const auto limit = block_limit();
    const auto start = now();

    // Targets estimated while the projection is held are of one pool.
    const auto held = estimate_.estimating &&
        (start - estimate_.estimated) < settings.estimate_cache();

    if (!held)
    {
        estimate_.projection = {};
        estimate_.arrived = false;
        estimate_.estimated = start;
    }

    const auto projected = held ?
        query.get_pool_continuation(estimate_.projection, count, limit,
            settings.maximum_chunk_depth, settings.bytes_per_sigop) :
        query.get_pool_projection(estimate_.projection, count, limit,
            settings.maximum_chunk_depth, settings.cutoff_rate_(),
            settings.bytes_per_sigop, settings.maximum_outcomes);

    report(pool_scan::projection, start);
    estimate_.estimating = !projected;
    if (projected)
    {
        handler(fault(projected), {});
        return;
    }

    // Arrivals are whole from the floor to the height of the open interval.
    if (mode != estimate_mode::basic && !estimate_.arrived)
    {
        const auto whole = floored_subtract(context_.pool.height,
            context_.floor);
        const auto heights = std::min(whole, conservative_estimate_window);
        if (const auto ec = query.get_pool_arrivals(estimate_.arrivals,
            context_.pool.height, heights, settings.bytes_per_sigop))
        {
            estimate_.estimating = false;
            handler(fault(ec), {});
            return;
        }

        estimate_.arrived = true;
    }

    const auto arrive = [&](uint64_t rate) NOEXCEPT
    {
        return arriving(estimate_.arrivals, rate, mode);
    };

    using algorithm = pool::algorithm;
    const auto space = limit.weight;
    const auto room = ceilinged_multiply<uint64_t>(settings.estimate_room,
        light_weight_factor);
    const auto rate = algorithm::estimate(estimate_.projection, count, space,
        room, arrive);
    if (rate == max_uint64)
    {
        handler(error::estimate_false, {});
        return;
    }

    handler(error::success, std::max(rate, settings.minimum_fee_rate_()));
}

// private
void chaser_transaction::do_get_confirmable_txs(
    const confirmable_txs_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, {});
        return;
    }

    database::tx_links links{};
    const auto start = now();
    const auto ec = archive().get_confirmable_txs(links);
    report(pool_scan::confirmable, start);
    handler(ec, links);
}

// private
void chaser_transaction::do_get_confirmable_fees(
    const confirmable_fees_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (!pooling_)
    {
        handler(error::pooling_disabled, {});
        return;
    }

    database::fee_rates rates{};
    const auto start = now();
    const auto ec = archive().get_confirmable_fees(rates);
    report(pool_scan::confirmable, start);
    handler(ec, rates);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
