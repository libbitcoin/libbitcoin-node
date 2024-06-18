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
    threadpool_(std::max(node.config().node.threads, 1_u32))
{
}

code chaser_confirm::start() NOEXCEPT
{
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

// Protected
// ----------------------------------------------------------------------------

bool chaser_confirm::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
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
            POST(do_validated, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::valid:
        {
            // value is individual height.
            POST(do_validated, possible_narrow_cast<height_t>(value));
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
// a stronger chain is considered. Currently total work at a given block is not
// archived, so this organization (like in the organizer) requires scanning to
// the fork point from the block and to the top confirmed from the fork point.
// The scans are extremely fast and tiny in all typical scnearios, so it may
// not improve performance or be worth spending 32 bytes per header to store
// work, especially since individual header work is obtained from 4 bytes.
void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || !fork_.empty())
        return;

    // TODO: update specialized fault codes.

    // Compute relative work.
    // ........................................................................

    bool strong{};
    uint256_t work{};

    if (!get_fork_work(work, fork_, height))
    {
        fault(error::get_fork_work);
        return;
    }

    if (!get_is_strong(strong, work, height))
    {
        fault(error::get_is_strong);
        return;
    }

    if (!strong)
    {
        fork_.clear();
        return;
    }

    // Pop down to the fork point.
    // ........................................................................

    auto& query = archive();
    const auto top = query.get_top_confirmed();
    fork_point_ = height - fork_.size();
    if (top < fork_point_)
    {
        fault(error::invalid_fork_point);
        return;
    }

    popped_.clear();
    for (auto index = top; index > fork_point_; --index)
    {
        const auto link = query.to_confirmed(index);
        if (link.is_terminal())
        {
            fault(error::to_confirmed);
            return;
        }

        popped_.push_back(link);
        if (!query.set_unstrong(link))
        {
            fault(error::node_confirm);
            return;
        }

        if (!query.pop_confirmed())
        {
            fault(error::pop_confirmed);
            return;
        }

        notify(error::success, chase::reorganized, popped_.back());
        fire(events::block_reorganized, index);
    }

    // Push candidate headers to confirmed chain.
    // ........................................................................

    do_organize(add1(fork_point_));
}

void chaser_confirm::do_organize(size_t height) NOEXCEPT
{
    if (fork_.empty())
        return;

    auto& query = archive();
    const auto& link = fork_.back();
    const auto ec = query.get_block_state(link);
    if (ec == database::error::integrity)
    {
        fault(ec);
        return;
    }

    if (ec == database::error::block_unconfirmable)
    {
        notify(ec, chase::unconfirmable, link);
        fire(events::block_unconfirmable, height);
        if (!roll_back(popped_, link, fork_point_, height))
        {
            fault(error::node_roll_back);
            return;
        }

        fork_.clear();
        return;
    }

    const auto is_strong = is_under_checkpoint(height) ||
        query.is_milestone(link);

    // Required for block_confirmable and all confirmed blocks.
    // Checkpoint and milestone guarantee set_strong is always set.
    if (!is_strong && !query.set_strong(link))
    {
        fault(error::set_confirmed);
        return;
    }

    if (ec == database::error::block_confirmable || is_strong)
    {
        // TODO: compute fees from validation records.
        if ((ec != database::error::block_confirmable) &&
            !query.set_block_confirmable(link, uint64_t{}))
        {
            fault(error::block_confirmable);
            return;
        }

        notify(ec, chase::confirmable, height);
        ////fire(events::confirm_bypassed, height);

        if (!set_organized(link, height))
        {
            fault(error::set_confirmed);
        }

        fork_.clear();
        return;
    }

    // TODO: the quantity of work must be throttled.
    // This will very rapidly pump all outstanding work into asio queue.
    if (!enqueue_block(link))
    {
        fault(error::node_validate);
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
    if ((ec = query.unspent_duplicates(txs.front(), ctx)))
    {
        POST(confirm_block, ec, link, ctx.height);
        return true;
    }

    if (is_one(txs.size()))
    {
        POST(confirm_block, ec, link, ctx.height);
        return true;
    }

    // race_unity: last to finish with success, or first error code.
    const auto racer = std::make_shared<race>(sub1(txs.size()));
    racer->start(BIND(handle_txs, _1, _2, link, ctx.height));
    ////fire(events::block_buffered, ctx.height);

    for (auto tx = std::next(txs.begin()); tx != txs.end(); ++tx)
        boost::asio::post(threadpool_.service(),
            std::bind(&chaser_confirm::confirm_tx,
                this, ctx, *tx, racer));

    return true;
}

// START WORK UNIT
void chaser_confirm::confirm_tx(const database::context& ctx,
    const tx_link& link, const race::ptr& racer) NOEXCEPT
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
    if (ec == error::store_integrity || ec == database::error::integrity)
        fault(error::node_validate);

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
        if (!roll_back(popped_, link, fork_point_, height))
        {
            fault(error::node_roll_back);
            return;
        }

        fork_.clear();
        return;
    }

    // TODO: compute fees from validation records.
    if (!query.set_block_confirmable(link, uint64_t{}))
    {
        fault(error::block_confirmable);
        return;
    }

    notify(error::success, chase::confirmable, height);
    fire(events::block_confirmed, height);
    if (!set_organized(link, height))
    {
        fault(error::set_confirmed);
        return;
    }

    LOGV("Block confirmed and organized: " << height);

    const auto next = add1(height);
    fork_.pop_back();
    if (!fork_.empty())
    {
        do_organize(next);
        return;
    }

    // Prevent stall by bumping, as the event may have been missed.
    const auto error = query.get_block_state(query.to_candidate(next));
    if ((error == database::error::block_valid) ||
        (error == database::error::block_confirmable))
    {
        do_validated(next);
    }
}

// Private
// ----------------------------------------------------------------------------

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
    const header_link& link, size_t fork_point, size_t top) NOEXCEPT
{
    auto& query = archive();

    // The current block is set_strong but not confirmed.
    if (!query.set_unstrong(link))
        return false;

    for (auto height = top; height > fork_point; --height)
        if (!set_reorganized(query.to_confirmed(height), height))
            return false;

    for (const auto& fk: views_reverse(popped))
        if (!reset_organized(fk, ++fork_point))
            return false;

    return true;
}

bool chaser_confirm::get_fork_work(uint256_t& fork_work,
    header_links& fork, height_t fork_top) const NOEXCEPT
{
    const auto& query = archive();
    fork.clear();

    // Walk down candidate index from fork_top to fork point (first confirmed).
    for (auto link = query.to_candidate(fork_top);
        !query.is_confirmed_block(link);
        link = query.to_candidate(--fork_top))
    {
        // Terminal candidate from validated link implies candidate regression.
        // This is ok, just means that the fork is no longer a candidate.
        if (link.is_terminal())
        {
            fork_work = zero;
            return true;
        }

        uint32_t bits{};
        if (!query.get_bits(bits, link))
            return false;

        fork.push_back(link);
        fork_work += system::chain::header::proof(bits);
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
        confirmed_work += system::chain::header::proof(bits);
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
