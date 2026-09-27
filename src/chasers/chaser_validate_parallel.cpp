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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <shared_mutex>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace database;
using namespace std::chrono;

// Parallel execution path (concurrent by block).
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    bool batched{}, capturing{}, pooled{};
    auto& query = archive();
    const auto start = network::logger::now();
    const auto current = !bypass && is_current_header(link);

    if (!query.get_context(ctx, link))
    {
        ec = error::validate3;
    }
    else if (current && ((ec = validate_pooled(pooled, link, ctx))))
    {
        if (!node::error::error_category::contains(ec) &&
            !query.set_block_unconfirmable(link))
            ec = error::validate12;
    }
    else if (pooled)
    {
        ec = complete_pooled(link, ctx);
    }
    else
    {
        // TODO: a current block that is not fully pooled revalidates all of
        // its txs, including those sufficiently pooled. A better fallback
        // would validate only the insufficiently pooled txs, relying on the
        // pool for the others (as validate_pooled does for all txs).

        // TODO: implement allocator parameter resulting in full allocation to
        // shared_ptr<block>, to optimize deallocate (12% of milestone/filter).
        const auto block = query.get_block(link, node_witness_);

        if (!block)
        {
            ec = error::validate2;
        }
        else if ((ec = populate(bypass, *block, ctx)))
        {
            if (!query.set_block_unconfirmable(link))
                ec = error::validate4;
        }
        else if ((ec = validate(batched, capturing, bypass, *block, link,
            ctx)))
        {
            if (!query.set_block_unconfirmable(link))
                ec = error::validate5;
        }
    }

    if (!ec && current)
    {
        const auto elapsed = network::logger::now() - start;
        fire(events::validate_usecs,
            duration_cast<microseconds>(elapsed).count());
    }

    --validate_backlog_;
    complete_block(ec, link, ctx.height, bypass, batched, capturing);
}

// helpers
// ----------------------------------------------------------------------------

code chaser_validate::populate(bool bypass, const chain::block& block,
    const chain::context& ctx) NOEXCEPT
{
    const auto& query = archive();

    if (bypass)
    {
        // Populating for filters only (no validation metadata required).
        block.populate(ctx);
        if (!query.populate_without_metadata(block))
            return system::error::missing_previous_output;
    }
    else
    {
        // Internal maturity and time locks are verified here because they are
        // the only necessary confirmation checks for internal spends.
        if (const auto ec = block.populate(ctx))
            return ec;

        // Metadata identifies internal spends allowing confirmation bypass.
        if (!query.populate_with_metadata(block))
            return system::error::missing_previous_output;
    }
    
    return error::success;
}

// A block with all txs pooled under a sufficient context requires only block
// checks, performed by the store. Insufficiency implies full validation.
code chaser_validate::validate_pooled(bool& pooled, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    const auto ec = archive().validate_pooled(link, ctx, subsidy_interval_,
        initial_subsidy_);

    pooled = !ec;
    if (!ec || ec == database::error::unvalidated)
        return error::success;

    // Store codes are faults, consensus codes imply block invalidity.
    return database::error::error_category::contains(ec) ?
        error::validate11 : ec;
}

// A pooled block is valid, so is produced only as required for filters.
code chaser_validate::complete_pooled(const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    if (filter_ || (ctx.height >= silent_start_height_))
    {
        bool batched{}, capturing{};
        constexpr auto bypass = true;
        const auto block = query.get_block(link, node_witness_);
        if (!block)
            return error::validate2;

        if (populate(bypass, *block, ctx))
            return error::validate11;

        if (const auto ec = validate(batched, capturing, bypass, *block, link,
            ctx))
            return ec;
    }

    // Valid must be set after set_prevouts, set_filter_body, and set_silent.
    return query.set_block_valid(link) ? error::success : error::validate10;
}

code chaser_validate::validate(bool& batched, bool& capturing, bool bypass,
    const chain::block& block, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();

    if (!bypass)
    {
        code ec{};
        if (((ec = block.check(false))) || ((ec = block.check(ctx, false))))
            return ec;

        if ((ec = block.accept(ctx, subsidy_interval_, initial_subsidy_)))
            return ec;

        // Initialize signature capture (appends to this thread's accumulators).
        const auto capture = get_capture(link);
        capturing = capture.enabled;

        ec = block.connect(ctx, capture);

        // At least one signature batch was attempted (batch completion).
        batched = capture.batched;

        // Commit (or discard) the captured signatures, marking the block prevalid
        // contingent on batch verification. The commit epoch is mutually
        // exclusive with batch verification (turnstile).
        if (capturing)
        {
            if (!ec && batched)
                ec = commit_capture(batched, link);
            else
                clear_capture();
        }

        if (ec)
            return ec;

        // Prevouts optimize confirmation.
        if (!query.set_prevouts(link, block))
            return error::validate7;
    }

    if (!query.set_filter_body(link, block))
        return error::validate8;

    if ((ctx.height >= silent_start_height_) &&
        !query.set_silent(link, block))
        return error::validate9;

    // Defer block state change when batched.
    // Valid must be set after set_prevouts, set_filter_body, and set_silent.
    if (!batched && !bypass && !query.set_block_valid(link))
        return error::validate10;

    return error::success;
}

} // namespace node
} // namespace libbitcoin
