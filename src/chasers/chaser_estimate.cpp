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
#include <bitcoin/node/chasers/chaser_estimate.hpp>

#include <atomic>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/estimator.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_estimate

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_estimate::chaser_estimate(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start/stop
// ----------------------------------------------------------------------------

code chaser_estimate::start() NOEXCEPT
{
    if (node_settings().fee_estimate_enabled())
    {
        SUBSCRIBE_CHASE(handle_chase, _1, _2, _3);
    }

    return error::success;
}

void chaser_estimate::stopping(const code& ec) NOEXCEPT
{
    stopping_.store(true);
    chaser::stopping(ec);
}

// methods
// ----------------------------------------------------------------------------

void chaser_estimate::estimate(size_t target, estimator::mode mode,
    estimate_handler&& handler) NOEXCEPT
{
    if (!node_settings().fee_estimate_enabled())
    {
        handler(error::estimate_disabled, {});
        return;
    }

    POST(do_estimate, target, mode, std::move(handler));
}

// private
void chaser_estimate::do_estimate(size_t target, estimator::mode mode,
    const estimate_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Check this under strand so that chase can initialize first.
    if (!initialized())
    {
        handler(error::estimate_premature, {});
        return;
    }

    const auto value = estimator_->estimate(target, mode);
    const auto ec = (value < to_unsigned(max_int64) ? error::success :
        error::estimate_false);

    // Successful value is always castable to int64_t.
    handler(ec, value);
}

size_t chaser_estimate::top_height() const NOEXCEPT
{
    return initialized() ? estimator_->top_height() : zero;
}

bool chaser_estimate::initialized() const NOEXCEPT
{
    return initialized_.load(std::memory_order_relaxed);
}

// event handlers
// ----------------------------------------------------------------------------
// protected

bool chaser_estimate::handle_chase(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Keep updating the fee accumulator (blocks continue organizing).
    ////if (suspended())
    ////    return true;

    switch (event_)
    {
        // chase::block is only sent when current. This is captured as a cheap
        // way to test currency for initialization. Once initialized it is not
        // used again. chase::organized is used instead, to ensure that there
        // are no push() gaps due to falling out of currency.
        case chase::block:
        {
            if (!initialized())
            {
                BC_ASSERT(std::holds_alternative<header_t>(value));
                POST(do_initialize, std::get<header_t>(value));
            }

            break;
        }
        case chase::organized:
        {
            if (initialized())
            {
                BC_ASSERT(std::holds_alternative<header_t>(value));
                POST(do_organized, std::get<header_t>(value));
            }

            break;
        }
        case chase::reorganized:
        {
            if (initialized())
            {
                BC_ASSERT(std::holds_alternative<header_t>(value));
                POST(do_reorganized, std::get<header_t>(value));
                break;
            }
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

void chaser_estimate::do_initialize(header_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (initialized())
        return;

    // Preempt initialize fault when horizon exceeds chain length.
    const auto horizon = node_settings().fee_estimate_horizon_();
    if (horizon > add1(archive().get_top_confirmed()))
        return;

    // Heap-allocate the estimator due to size.
    estimator_ = std::make_unique<estimator>();
    if (!estimator_->initialize(stopping_, archive(), horizon))
    {
        fault(error::estimates_initialize);
        estimator_.release();
        return;
    }

    initialized_.store(true, std::memory_order_relaxed);
}

void chaser_estimate::do_organized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (initialized())
    {
        const auto& query = archive();
        const auto height = query.get_height(link);

        if (height.is_terminal())
        {
            fault(error::estimates_push1);
            return;
        }

        // Organization events backlog during initialization.
        if (height.value <= estimator_->top_height())
            return;

        if (!estimator_->push(query))
            fault(error::estimates_push2);
    }
}

void chaser_estimate::do_reorganized(header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (initialized())
    {
        const auto& query = archive();
        const auto height = query.get_height(link);

        if (height.is_terminal())
        {
            fault(error::estimates_pop1);
            return;
        }

        // Organization events backlog during initialization.
        if (height.value > estimator_->top_height())
            return;

        if (!estimator_->push(query))
            fault(error::estimates_pop2);
    }
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
