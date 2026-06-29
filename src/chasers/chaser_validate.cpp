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
    silent_start_height_(node.node_settings().silent_start_height),
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

    set_position(archive().get_fork());
    if (const auto ec = start_batch())
        return fault(ec);

    SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Latch recovering from disk full, before suspension is lifted.
    // Because in-flight blocks are lost, reset position when backlog clears.
    if (event_ == chase::unfull)
    {
        recovering_.store(true);
        return true;
    }

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

    if (recovering_.load() && is_zero(validate_backlog_.load()))
    {
        recovering_.store(false);
        set_position(archive().get_fork());
    }

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
    while ((validate_backlog_ < maximum_backlog_) && !closed() && !suspended())
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Must exit on unassociated so they are not set valid in bypass.
        // Given height-based iteration, any block state may be enountered.
        if (ec == database::error::unassociated)
            return;

        // The last job requiring validation has been posted.
        if (height == maximum_height_)
            maximum_posted_ = true;

        const auto bypass = is_under_checkpoint(height) ||
            query.is_milestone(link);

        switch (ec.value())
        {
            case database::error::unvalidated:
            case database::error::unknown_state:
            {
                if (bypass && !filter_)
                {
                    complete_block(error::success, link, height, true);
                }
                else
                {
                    ++validate_backlog_;
                    post_block(link, bypass);
                }
                break;
            }
            case database::error::block_valid:
            case database::error::block_confirmable:
            {
                complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_prevalid:
            {
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

        // All posted validations must complete or this is invalid.
        // So posted validations continue despite network suspension.
        set_position(height++);
    }
}

void chaser_validate::post_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    BC_ASSERT(stranded());
    PARALLEL(validate_block, link, bypass);
}

// May be either concurrent or stranded.
void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height, bool bypass, bool batched, bool faulted,
    bool capturing) NOEXCEPT
{
    // Not stranded when called from validate_block.
    if (is_zero(validate_backlog_.load()) && !stranded())
    {
        // Prevent stall by posting internal event, avoiding external handlers.
        handle_chase({}, chase::bump, height_t{});
    }

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
    {
        notify_block({}, height, link, bypass);
    }

    // Batch jobs (all posting from unstranded).
    // ------------------------------------------------------------------------

    // Faulted implies disk full prevented threshold batch writes.
    if (closed() || !batch_enabled_ || faulted)
        return;

    // Queue block and process batch if ready.
    if (batched)
    {
        ++batch_backlog_;
        POST(push_batch, link, height);
        return;
    }

    // Capturing disabled when confirmed chain current (and not under bypass).
    // When not in effect must drain last batch by last block validation.
    const auto current = !capturing && !bypass;

    // Drain batch when recent (current, or maximum reached without backlog).
    if (current || is_residual())
    {
        POST(process_batch, true);
    }
}

void chaser_validate::notify_block(const code& ec, size_t height, 
    const header_link& link, bool bypass, bool startup) NOEXCEPT
{
    // Not stranded when complete_block is called from validate_block.

    if (ec)
    {
        // INVALID BLOCK (not a fault but discontinue)
        if (!startup) notify(ec, chase::unvalid, link);
        fire(events::block_unconfirmable, height);
        LOGR("Invalid block [" << height << "] " << ec.message());
        return;
    }

    // VALID BLOCK
    if (!startup) notify(ec, chase::valid, possible_wide_cast<height_t>(height));
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
