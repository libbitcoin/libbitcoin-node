/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/chasers/chaser_confirm.hpp>

#include <functional>
#include <utility>
#include <bitcoin/database.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_confirm

using namespace system;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_confirm::chaser_confirm(full_node& node) NOEXCEPT
  : chaser(node),
    threadpool_(one)
{
}

code chaser_confirm::start() NOEXCEPT
{
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

void chaser_confirm::stopping(const code& ec) NOEXCEPT
{
    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_confirm::stop() NOEXCEPT
{
    if (!threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

// Protected
// ----------------------------------------------------------------------------

bool chaser_confirm::handle_event(const code&, chase event_,
    event_value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating message/query traffic from the validation messages.
    if (suspended())
        return true;

    // These can come out of order, advance in order synchronously.
    switch (event_)
    {
        case chase::blocks:
        {
            // TODO: value is branch point.
            ////BC_ASSERT(std::holds_alternative<height_t>(value));
            ////POST(do_validated, std::get<height_t>(value));
            break;
        }
        case chase::valid:
        {
            // value is individual height.
            ////BC_ASSERT(std::holds_alternative<height_t>(value));
            ////POST(do_validated, std::get<height_t>(value));
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

// confirm
// ----------------------------------------------------------------------------
// Blocks are either confirmed (blocks first) or validated/confirmed
// (headers first) at this point. An unconfirmable block may not land here.
// Candidate chain reorganizations will result in reported heights moving
// in any direction. Each is treated as independent and only one representing
// a stronger chain is considered.

// Compute relative work, set fork_ and fork_point_.
void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || busy())
        return;

    bool strong{};
    uint256_t work{};

    // Scan down from height to first confirmed, accumulate links and sum work.
    if (!get_fork_work(work, fork_, height))
    {
        fault(error::get_fork_work);
        return;
    }

    // No longer a candidate fork (heights are not candidates).
    if (fork_.empty())
        return;

    // fork_point is the highest candidate-confirmed common block.
    fork_point_ = height - fork_.size();
    if (!get_is_strong(strong, work, fork_point_))
    {
        fault(error::get_is_strong);
        return;
    }

    // Not yet a strong fork (confirmed branch has at least as much work).
    if (!strong)
    {
        reset();
        return;
    }

    do_reorganize(archive().get_top_confirmed());
}

// Pop confirmed chain from height down to above fork point, save popped_.
void chaser_confirm::do_reorganize(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (height < fork_point_)
    {
        fault(error::invalid_fork_point);
        return;
    }

    popped_.clear();
    auto& query = archive();
    while (height > fork_point_)
    {
        const auto link = query.to_confirmed(height);
        if (link.is_terminal())
        {
            fault(error::to_confirmed);
            return;
        }

        popped_.push_back(link);
        if (!query.set_unstrong(link))
        {
            fault(error::set_unstrong);
            return;
        }

        if (!query.pop_confirmed())
        {
            fault(error::pop_confirmed);
            return;
        }

        notify(error::success, chase::reorganized, popped_.back());
        fire(events::block_reorganized, height--);
    }

    do_organize(add1(height));
}

// Push candidate headers from above fork point to confirmed chain.
void chaser_confirm::do_organize(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (fork_.empty())
        return;

    auto& query = archive();
    bool confirmable = false;
    const auto& link = fork_.back();
    const auto bypass = is_under_checkpoint(height) ||
        query.is_milestone(link);

    if (!bypass)
    {
        // database::error::unassociated
        // database::error::block_unconfirmable
        // database::error::block_confirmable
        // database::error::block_valid
        // database::error::unknown_state
        // database::error::unvalidated
        const auto ec = query.get_block_state(link);

        // Previously unconfirmable block.
        if (ec == database::error::block_unconfirmable)
        {
            notify(ec, chase::unconfirmable, link);
            fire(events::block_unconfirmable, height);

            // Roll back previously confirmed blocks.
            if (!roll_back(popped_, fork_point_, sub1(height)))
                fault(error::node_roll_back);

            reset();
            return;
        }

        // Previously evaluated and set confirmable block.
        if (ec == database::error::block_confirmable)
        {
            // Required of all confirmed, and before checking confirmable.
            // Checked blocks are set at download, cannot be unset (reorged).
            // Milestone blocks are set/unset strong by header organization.
            if (!query.set_strong(link))
            {
                fault(error::set_strong);
                return;
            }

            confirmable = true;
        }
    }

    if (bypass || confirmable)
    {
        notify(error::success, chase::confirmable, height);
        ////fire(events::confirm_bypassed, height);

        if (!set_organized(link, height))
        {
            fault(error::set_organized);
            return;
        }

        POST(next_block, add1(height));
        return;
    }

    if (!enqueue_block(link))
    {
        fault(error::node_confirm);
        return;
    }
}

// DISTRUBUTE WORK UNITS
bool chaser_confirm::enqueue_block(const header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    context ctx{};
    const auto txs = query.to_transactions(link);
    if (txs.empty() || !query.get_context(ctx, link))
        return false;

    code ec{};
    const auto height = ctx.height;
    if ((ec = query.unspent_duplicates(txs.front(), ctx)))
    {
        POST(confirm_block, ec, link, height);
        return true;
    }

    if (is_one(txs.size()))
    {
        POST(confirm_block, ec, link, height);
        return true;
    }

    const auto racer = std::make_shared<race>(sub1(txs.size()));
    racer->start(BIND(handle_txs, _1, _2, link, height));
    ////fire(events::block_buffered, height);

    for (auto tx = std::next(txs.begin()); tx != txs.end(); ++tx)
        boost::asio::post(threadpool_.service(),
            BIND(confirm_tx, ctx, *tx, racer));

    return true;
}

// START WORK UNIT
void chaser_confirm::confirm_tx(const context& ctx, const tx_link& link,
    const race::ptr& racer) NOEXCEPT
{
    const auto ec = archive().tx_confirmable(link, ctx);
    POST(handle_tx, ec, link, racer);
}

// FINISH WORK UNIT
void chaser_confirm::handle_tx(const code& ec, const tx_link& tx,
    const race::ptr& racer) NOEXCEPT
{
    BC_ASSERT(stranded());

    // handle_txs will only get invoked once, with a first error code, so
    // invoke fault here ensure that non-validation codes are not lost.
    if (ec == database::error::integrity)
        fault(error::node_confirm);

    // TODO: need to sort out bypass, validity, and fault codes.
    // Always allow the racer to finish, invokes handle_txs exactly once.
    racer->finish(ec, tx);
}

// SYNCHRONIZE WORK UNITS
void chaser_confirm::handle_txs(const code& ec, const tx_link& tx,
    const header_link& link, size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        // Log tx here as it's the first failed one.
        LOGR("Error confirming tx [" << encode_hash(archive().get_tx_key(tx))
            << "] " << ec.message());
    }

    confirm_block(ec, link, height);
}

// SUMMARIZE WORK
void chaser_confirm::confirm_block(const code& ec, const header_link& link,
    size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    if (ec == database::error::integrity)
    {
        fault(error::node_confirm);
        return;
    }

    if (ec)
    {
        if (!query.set_block_unconfirmable(link))
        {
            fault(error::set_block_unconfirmable);
            return;
        }

        LOGR("Unconfirmable block [" << height << "] " << ec.message());
        notify(ec, chase::unconfirmable, link);
        fire(events::block_unconfirmable, height);

        // Roll back current and previously confirmed blocks.
        if (!roll_back(popped_, fork_point_, height, link))
        {
            fault(error::node_roll_back);
            return;
        }

        reset();
        return;
    }

    // TODO: move fee setter to set_block_valid (transitory) and propagate to
    // TODO: set_block_confirmable (final). Bypassed do not have the fee cache.
    if (!query.set_block_confirmable(link, uint64_t{}))
    {
        fault(error::set_block_confirmable);
        return;
    }

    notify(error::success, chase::confirmable, height);
    fire(events::block_confirmed, height);
    if (!set_organized(link, height))
    {
        fault(error::set_organized);
        return;
    }

    LOGV("Block confirmed and organized: " << height);
    next_block(add1(height));
}

// SHARED ITERATE
void chaser_confirm::next_block(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Continue until fork is empty.
    fork_.pop_back();
    if (!fork_.empty())
    {
        do_organize(height);
        return;
    }

    reset();
    const auto& query = archive();

    // Prevent stall by bumping, as the event may have been missed.
    const auto code = query.get_block_state(query.to_candidate(height));
    if ((code == database::error::block_valid) ||
        (code == database::error::block_confirmable))
    {
        do_validated(height);
    }
}

// Private
// ----------------------------------------------------------------------------

void chaser_confirm::reset() NOEXCEPT
{
    fork_.clear();
    popped_.clear();
    fork_point_ = zero;
}

bool chaser_confirm::busy() const NOEXCEPT
{
    return !fork_.empty();
}

bool chaser_confirm::set_organized(const header_link& link, height_t) NOEXCEPT
{
    auto& query = archive();
    if (!query.push_confirmed(link))
        return false;

    notify(error::success, chase::organized, link);
    ////fire(events::block_organized, height);
    return true;
}

bool chaser_confirm::reset_organized(const header_link& link,
    height_t height) NOEXCEPT
{
    auto& query = archive();
    if (!query.set_strong(link) || !query.push_confirmed(link))
        return false;

    notify(error::success, chase::organized, link);
    fire(events::block_organized, height);
    return true;
}

bool chaser_confirm::set_reorganized(const header_link& link,
    height_t height) NOEXCEPT
{
    auto& query = archive();
    if (!query.pop_confirmed() || !query.set_unstrong(link))
        return false;

    notify(error::success, chase::reorganized, link);
    fire(events::block_reorganized, height);
    return true;
}

bool chaser_confirm::roll_back(const header_links& popped,
    size_t fork_point, size_t top, const header_link& link) NOEXCEPT
{
    // The current block (top/link) is set_strong but is not confirmable.
    return archive().set_unstrong(link) && roll_back(popped, fork_point, top);
}

bool chaser_confirm::roll_back(const header_links& popped, size_t fork_point,
    size_t top) NOEXCEPT
{
    const auto& query = archive();

    for (auto height = top; height > fork_point; --height)
        if (!set_reorganized(query.to_confirmed(height), height))
            return false;

    for (const auto& fk: views_reverse(popped))
        if (!reset_organized(fk, ++fork_point))
            return false;

    return true;
}

bool chaser_confirm::get_fork_work(uint256_t& fork_work, header_links& fork,
    height_t fork_top) const NOEXCEPT
{
    const auto& query = archive();
    header_link link{};
    fork_work = zero;
    fork.clear();

    // Walk down candidates from fork_top to fork point (highest common).
    for (link = query.to_candidate(fork_top);
        !link.is_terminal() && !query.is_confirmed_block(link);
        link = query.to_candidate(--fork_top))
    {
        uint32_t bits{};
        if (!query.get_bits(bits, link))
            return false;

        fork_work += chain::header::proof(bits);
        fork.push_back(link);
    }

    // Terminal candidate from validated link implies candidate regression.
    // This is ok, it just means that the fork is no longer a candidate.
    if (link.is_terminal())
    {
        fork_work = zero;
        fork.clear();
    }

    return true;
}

// A fork with greater work will cause confirmed reorganization.
bool chaser_confirm::get_is_strong(bool& strong, const uint256_t& fork_work,
    size_t fork_point) const NOEXCEPT
{
    uint256_t confirmed_work{};
    const auto& query = archive();

    for (auto height = query.get_top_confirmed(); height > fork_point;
        --height)
    {
        uint32_t bits{};
        if (!query.get_bits(bits, query.to_confirmed(height)))
            return false;

        // Not strong when confirmed_work equals or exceeds fork_work.
        confirmed_work += chain::header::proof(bits);
        if (confirmed_work >= fork_work)
        {
            strong = false;
            return true;
        }
    }

    strong = true;
    return true;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
