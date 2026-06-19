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
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Independent threadpool and strand (base class strand uses network pool).
chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    validation_threadpool_(node.node_settings().threads_(),
        node.node_settings().thread_priority_()),
    validation_strand_(validation_threadpool_.service().get_executor()),
    subsidy_interval_(node.system_settings().subsidy_interval_blocks),
    initial_subsidy_(node.system_settings().initial_subsidy()),
    maximum_backlog_(node.node_settings().maximum_concurrency_()),
    maximum_height_(node.node_settings().maximum_height_()),
    batch_target_(node.node_settings().batch_signatures),
    batch_enabled_(node.node_settings().batch_signatures_enabled()),
    node_witness_(node.network_settings().witness_node()),
    filter_(node.archive().filter_enabled())
{
}

code chaser_validate::start() NOEXCEPT
{
    if (!node_settings().headers_first)
        return error::success;

    if (const auto ec = start_batch())
        return ec;

    set_position(archive().get_fork());
    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    // Incoming events may already be flushed to the strand at this point.
    if (suspended())
        return true;

    switch (event_)
    {
        case chase::resume:
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_checked, std::get<height_t>(value));
            break;
        }
        case chase::regressed:
        case chase::disorganized:
        {
            // value is regression branch_point.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_regressed, std::get<height_t>(value));
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

// Track downloaded
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (branch_point >= position())
        return;

    set_position(branch_point);
}

void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Cannot validate next block until all previous blocks are archived.
    if (height == add1(position()))
        do_bumped(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto height = add1(position());
    if (archive().is_validateable(height))
        do_bumped(height);
}

// Validate (cancellable)
// ----------------------------------------------------------------------------

void chaser_validate::do_bumped(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Bypass until next event if validation backlog is full.
    // Stop when suspended as write error does not terminate asynchronous loop.
    while ((backlog_ < maximum_backlog_) && !closed() && !suspended())
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Must exit on unassociated so they are not set valid in bypass.
        // Given height-based iteration, any block state may be enountered.
        if (ec == database::error::unassociated)
            return;

        const auto bypass = is_under_checkpoint(height) ||
            query.is_milestone(link);

        switch (ec.value())
        {
            case database::error::unvalidated:
            case database::error::unknown_state:
            {
                if (!bypass || filter_)
                    post_block(link, bypass);
                else
                    complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_valid:
            case database::error::block_confirmable:
            {
                complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_unconfirmable:
            {
                return;
            }
            ////case database::error::unassociated
            default:
            {
                fault(error::validate1);
                return;
            }
        }

        if (height == maximum_height_)
        {
            // TODO: the height limit has been queued for validation.
            // TODO: once all queued validations are complete/batched, invoke
            // TODO: POST(process_batch(true)) from point where that is known.
        }

        // All posted validations must complete or this is invalid.
        // So posted validations continue despite network suspension.
        set_position(height++);
    }
}

void chaser_validate::post_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    BC_ASSERT(stranded());

    backlog_.fetch_add(one, relaxed);
    PARALLEL(validate_block, link, bypass);
}

// Unstranded (concurrent by block)
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    bool batched{}, faulted{}, enabled{};
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
    else if ((ec = validate(batched, faulted, enabled, bypass, *block, link, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate5;
    }

    complete_block(ec, link, ctx.height, bypass, batched, faulted, enabled);

    // Backlog does not account for post-validation (batch) processing.
    // Prevent stall by posting internal event, avoiding external handlers.
    if (is_one(backlog_.fetch_sub(one, relaxed)))
        handle_chase({}, chase::bump, height_t{});
}

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

    // Defer block state change when batched (or faulted).
    // Valid must be set after set_prevouts and set_filter_body.
    if (!batched && !bypass && !query.set_block_valid(link))
        return error::validate8;

    return error::success;
}

// May be either concurrent or stranded.
void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height, bool bypass, bool batched, bool faulted,
    bool capturing) NOEXCEPT
{
    // Node errors are fatal (or disk full recoverable).
    if (ec && node::error::error_category::contains(ec))
    {
        LOGF("Fault validating [" << height << "] " << ec.message());
        fault(ec);
        return;
    }

    // Prioritize non-signature validation failures over batch result.
    if (ec)
    {
        notify_block(ec, height, link, bypass);
        return;
    }

    // Falls through to trigger residual batch processing.
    if (!batched)
        notify_block({}, height, link, bypass);

    // Batch jobs.
    // ------------------------------------------------------------------------
    // Avoid posting new work when closing.
    if (closed() || !batch_enabled_)
        return;

    // Trigger residual block batch processing.
    if (!capturing && !bypass)
    {
        POST(process_batch, true);
        return;
    }

    // Retry faulted threshold (presumes disk full).
    if (faulted)
    {
        POST(post_block, link, bypass);
        return;
    }

    // Queue block and process batch if ready.
    if (batched)
    {
        POST(push_batch, link, height);
        return;
    }
}

void chaser_validate::notify_block(const code& ec, size_t height, 
    const header_link& link, bool bypass) NOEXCEPT
{
    if (ec)
    {
        // INVALID BLOCK (not a fault but discontinue)
        notify(ec, chase::unvalid, link);
        fire(events::block_unconfirmable, height);
        LOGR("Invalid block [" << height << "] " << ec.message());
        return;
    }

    // VALID BLOCK
    notify(ec, chase::valid, possible_wide_cast<height_t>(height));
    fire(events::block_validated, height);
    LOGV("Block validated: " << height << (bypass ? " (bypass)" : ""));
}

// Overrides due to independent priority thread pool
// ----------------------------------------------------------------------------

void chaser_validate::stopping(const code& ec) NOEXCEPT
{
    // Stop long-running batch validations.
    stopping_.store(true);

    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    validation_threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_validate::stop() NOEXCEPT
{
    if (!validation_threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

network::asio::strand& chaser_validate::strand() NOEXCEPT
{
    return validation_strand_;
}

bool chaser_validate::stranded() const NOEXCEPT
{
    return validation_strand_.running_in_this_thread();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
