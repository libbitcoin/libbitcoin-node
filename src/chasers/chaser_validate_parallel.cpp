/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

// Parallel execution path (concurrent by block).
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    bool batched{}, faulted{}, capturing{};
    auto& query = archive();

    // TODO: implement allocator parameter resulting in full allocation to
    // shared_ptr<block>, to optimize deallocate (12% of milestone/filter).
    const auto block = query.get_block(link, node_witness_);

    if (!block)
    {
        ec = error::validate2;
    }
    else if (!query.get_context(ctx, link))
    {
        ec = error::validate3;
    }
    else if ((ec = populate(bypass, *block, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate4;
    }
    else if ((ec = validate(batched, faulted, capturing, bypass, *block, link,
        ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate5;
    }

    --validate_backlog_;
    complete_block(ec, link, ctx.height, bypass, batched, faulted, capturing);
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

code chaser_validate::validate(bool& batched, bool& faulted, bool& capturing,
    bool bypass, const chain::block& block, const header_link& link,
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

        // Initialize block capture.
        const auto capture = get_capture(link);

        // Signature capture is enabled.
        capturing = capture.enabled;

        // This critical section is mutually-exclusive with batch verification.
        // ====================================================================
        {
            std::shared_lock lock{ mutex_, std::defer_lock };
            if (capturing) lock.lock();

            // Sequentially connect block with signature capture (if enabled).
            // There is not stop during connect, so shutdown will wait on the
            // completion (block consistency) of all signature captures. But
            // the faulted state of batch is not persisted (because disk full).
            if ((ec = block.connect(ctx, capture)))
                return ec;

            // At least one signature batch was attempted (defer completion).
            batched = capture.batched;

            // Threshold batch commit failed, block otherwise passed (retry).
            faulted = capture.faulted;
        }
        // ================================================================

        // Prevouts optimize confirmation.
        // Block will be retried if batch is faulted.
        if (!faulted && !query.set_prevouts(link, block))
            return error::validate6;
    }

    // Block will be retried if batch is faulted.
    if (!faulted && !query.set_filter_body(link, block))
        return error::validate7;

    // Block will be retried if batch is faulted.
    if (!faulted && (ctx.height >= silent_start_height_) &&
        !query.set_silent(link, block))
        return error::validate8;

    // Defer block state change when batched (or faulted).
    // Valid must be set after set_prevouts, set_filter_body, and set_silent.
    if (!batched && !bypass && !query.set_block_valid(link))
        return error::validate9;

    return error::success;
}

} // namespace node
} // namespace libbitcoin
