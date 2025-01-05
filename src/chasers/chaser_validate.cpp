/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

#include <functional>
#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace database;
using namespace std::placeholders;

// Shared pointer is required to keep the race object alive in bind closure.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Multiple higher priority thread strand (base class strand uses network pool).
// Higher priority than downloader (net) ensures locality to downloader writes.
chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    concurrent_(node.config().node.concurrent_validation),
    maximum_backlog_(node.config().node.maximum_concurrency_()),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_(node.config().bitcoin.subsidy_interval_blocks),
    threadpool_(node.config().node.threads_(), node.config().node.priority_()),
    independent_strand_(threadpool_.service().get_executor())
{
}

code chaser_validate::start() NOEXCEPT
{
    const auto& query = archive();
    set_position(query.get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

void chaser_validate::stopping(const code& ec) NOEXCEPT
{
    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_validate::stop() NOEXCEPT
{
    if (!threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

bool chaser_validate::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // TODO: resume needs to ensure a bump.
    // Stop generating query during suspension.
    if (suspended())
        return true;

    // These come out of order, advance in order asynchronously.
    // Asynchronous completion again results in out of order notification.
    switch (event_)
    {
        case chase::start:
        case chase::bump:
        {
            if (concurrent_ || mature_)
            {
                POST(do_bump, height_t{});
            }

            break;
        }
        case chase::checked:
        {
            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));

            if (concurrent_ || mature_)
            {
                POST(do_checked, std::get<height_t>(value));
            }

            break;
        }
        case chase::regressed:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_regressed, std::get<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
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

// track downloaded
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (branch_point >= position())
        return;

    // Update position and wait.
    set_position(branch_point);
}

// Candidate block checked at given height, if next then validate/advance.
void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (height == add1(position()))
        do_bump(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Bypass until next event if validation backlog is full.
    for (auto height = add1(position()); unfilled() && !closed(); ++height)
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Wait until the gap is filled at a current height (or next top).
        if (ec == database::error::unassociated)
            return;

        // The size of the job is not relevant to the backlog cost.
        ++backlog_;

        if (ec == database::error::block_unconfirmable)
        {
            complete_block(ec, link, height);
            return;
        }

        set_position(height);

        if ((ec == database::error::block_valid) ||
            (ec == database::error::block_confirmable) ||
            is_under_checkpoint(height) || query.is_milestone(link))
        {
            complete_block(error::success, link, height);
            continue;
        }

        PARALLEL(validate_block, link);
    }
}

// Unstranded (concurrent by block).
void chaser_validate::validate_block(const header_link& link) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    auto& query = archive();
    const auto block = query.get_block(link);

    if (!block)
    {
        ec = error::validate1;
    }
    else if (!query.get_context(ctx, link))
    {
        ec = error::validate2;
    }
    else if (!block->populate())
    {
        ec = system::error::relative_time_locked;
    }
    else if (!query.populate(*block))
    {
        ec = system::error::missing_previous_output;
    }
    else if ((ec = block->accept(ctx, subsidy_interval_, initial_subsidy_)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate3;
    }
    else if ((ec = block->connect(ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate4;
    }
    else if (!query.set_block_valid(link, block->fees()))
    {
        ec = error::validate5;
    }
    else if (!query.set_prevouts(ctx.height, *block))
    {
        ec = error::validate6;
    }
    else if (query.set_filter_body(link, *block))
    {
        ec = error::validate7;
    }
    else
    {
        fire(events::block_validated, ctx.height);
    }

    // Return to strand to handle result.
    POST(complete_block, ec, link, ctx.height);
}

void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // The size of the job is not relevant to the backlog cost.
    --backlog_;

    if (ec)
    {
        // Differentiated fault codes for troubleshooting.
        if (ec == error::validate1 ||
            ec == error::validate2 ||
            ec == error::validate3 ||
            ec == error::validate4 ||
            ec == error::validate5 ||
            ec == error::validate6 ||
            ec == error::validate7)
        {
            fault(ec);
            return;
        }

        LOGR("Unconfirmable block [" << height << "] " << ec.message());
        fire(events::block_unconfirmable, height);
        notify(ec, chase::unvalid, link);
        return;
    }

    notify(ec, chase::valid, possible_wide_cast<height_t>(height));

    // Prevent stall by posting internal event, avoid hitting external handlers.
    if (is_zero(backlog_))
        handle_event(ec, chase::bump, height_t{});
}

// Strand.
// ----------------------------------------------------------------------------

network::asio::strand& chaser_validate::strand() NOEXCEPT
{
    return independent_strand_;
}

bool chaser_validate::stranded() const NOEXCEPT
{
    return independent_strand_.running_in_this_thread();
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
