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

// Non-coinbase txs of the block that cannot be validated from the pool.
static size_t unpooled_count(const query& query, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    size_t count{};
    const auto txs = query.to_transactions(link);
    if (txs.empty())
        return count;

    for (auto tx = std::next(txs.cbegin()); tx != txs.cend(); ++tx)
    {
        pooled_tx out{};
        out.prevouts.resize(query.input_count(*tx));
        if (query.get_pooled(out, *tx, context::from(ctx)))
            ++count;
    }

    return count;
}

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
    else if (bypass && !filter_ && query.is_silent(link, ctx.height))
    {
        ec = validate_silent(link, ctx);
    }
    else
    {
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
        const auto usecs = duration_cast<microseconds>(elapsed).count();
        fire(events::validate_usecs, usecs);
        if (pooled)
        {
            LOGN("Validated pooled block [" << ctx.height << "] of ("
                << query.get_tx_count(link) << ") txs in (" << usecs
                << ") usecs.");
        }
        else
        {
            LOGN("Validated full block [" << ctx.height << "] of ("
                << query.get_tx_count(link) << ") txs with ("
                << unpooled_count(query, link, ctx) << ") unpooled in ("
                << usecs << ") usecs.");
        }
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
        // Populating for optional indexes only (no validation metadata).
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

// Bypassed without client filters, a block is read only for its silent
// payment records, so it is not materialized.
code chaser_validate::validate_silent(const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    auto wire = query.get_wire_block(link, node_witness_);
    chain::view::block block{ std::move(wire), node_witness_ };

    if (!block.is_valid())
        return error::validate2;

    // Only the prevouts of silent payment eligible txs are populated.
    data_chunk prevouts{};
    std::vector<bool> selected{};
    if (!query.get_silent_prevouts(prevouts, selected, link, block) ||
        !block.populate(std::move(prevouts), selected))
        return query.set_block_unconfirmable(link) ?
            code{ system::error::missing_previous_output } : error::validate4;

    const auto committed = is_silent_capturing(link) &&
        commit_silent_batch(link, block, ctx.height);

    if (!committed && !query.set_silent(link, block))
        return error::validate9;

    return query.set_block_valid(link) ? error::success : error::validate10;
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

// A pooled block is valid, so is produced only as required for indexes.
code chaser_validate::complete_pooled(const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    if (filter_ || query.is_silent(link, ctx.height))
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

    // Valid must be set after set_prevouts and set_filter_body.
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

    // Banked records are set when the bank drains.
    if (query.is_silent(link, ctx.height))
    {
        const auto committed = is_silent_capturing(link) &&
            commit_silent_batch(link, block, ctx.height);

        if (!committed && !query.set_silent(link, block))
            return error::validate9;
    }

    // Defer block state change when batched.
    // Valid must be set after set_prevouts and set_filter_body.
    if (!batched && !query.set_block_valid(link))
        return error::validate10;

    return error::success;
}

} // namespace node
} // namespace libbitcoin
