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
#include <bitcoin/node/chasers/chaser_confirm.hpp>

#include <ranges>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
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
  : chaser(node)
{
}

code chaser_confirm::start() NOEXCEPT
{
    const auto& query = archive();
    set_position(query.get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_confirm::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    if (suspended())
        return true;

    switch (event_)
    {
        ////case chase::blocks:
        ////{
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////
        ////    // TODO: value is branch point.
        ////    POST(do_validated, std::get<height_t>(value));
        ////    break;
        ////}
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::valid:
        {
            // value is validated block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_validated, std::get<height_t>(value));
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

// track validation
// ----------------------------------------------------------------------------

void chaser_confirm::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Update position and wait.
    // Candidate chain is controlled by organizer and does not directly affect
    // confirmed chain. Organizer sets unstrong and then pops candidates.
    set_position(branch_point);
}

void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Cannot confirm next block until previous block is confirmed.
    if (height == add1(position()))
        do_bumped(height);
}

void chaser_confirm::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Only necessary when bumping as next position may not be validated.
    const auto height = add1(position());
    const auto link = query.to_candidate(height);
    const auto ec = query.get_block_state(link);
    const auto bypass =
        (ec == database::error::block_valid) ||
        (ec == database::error::block_confirmable) ||
        is_under_checkpoint(height) || query.is_milestone(link);

    // Block state must be checked, milestone, valid, or confirmable. This
    // is assured by chasing validations. But if filtering, must have been
    // filtered by the validator (otherwise bypass races ahead of validate).
    if (bypass && query.is_filtered(link))
        do_bumped(height);
}

// confirm (not cancellable)
// ----------------------------------------------------------------------------

// Compute relative work, set fork and fork_point, and invoke reorganize.
void chaser_confirm::do_bumped(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    // Scan from candidate height to first confirmed, return links and work.
    uint256_t work{};
    header_links fork{};
    if (!get_fork_work(work, fork, height))
    {
        fault(error::confirm2);
        return;
    }

    // No longer a candidate fork (may have been reorganized).
    if (fork.empty())
        return;

    bool strong{};
    const auto fork_point = height - fork.size();
    if (!get_is_strong(strong, work, fork_point))
    {
        fault(error::confirm3);
        return;
    }

    // Fork does not have more work than the confirmed branch.
    if (!strong)
        return;

    reorganize(fork, fork_point);
}

// Pop confirmed chain from top down to above fork point, save popped.
void chaser_confirm::reorganize(header_links& fork, size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto& query = archive();
    auto height = query.get_top_confirmed();
    if (height < fork_point)
    {
        fault(error::confirm4);
        return;
    }

    header_links popped{};
    while (height > fork_point)
    {
        const auto link = query.to_confirmed(height);
        if (link.is_terminal())
        {
            fault(error::confirm5);
            return;
        }

        popped.push_back(link);
        if (!set_reorganized(link, height--))
        {
            fault(error::confirm6);
            return;
        }
    }

    // Top is now fork_point.
    organize(fork, popped, fork_point);
}

// Push candidates (fork) from above fork point to confirmed chain.
// Restore popped from fork point if any candidate fails to confirm.
// Fork is always shortest candidate chain stronger than confirmed chain.
// No bump required upon complete since fully stranded (no message loss).
void chaser_confirm::organize(header_links& fork, const header_links& popped,
    size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    auto height = add1(fork_point);
    while (!fork.empty())
    {
        const auto& link = fork.back();
        auto ec = query.get_block_state(link);
        const auto bypass =
            (ec == database::error::block_confirmable) ||
            is_under_checkpoint(height) || query.is_milestone(link);

        if (bypass)
        {
            if (!query.set_filter_head(link))
            {
                fault(error::confirm7);
                return;
            }
        }
        else if (ec == database::error::block_valid)
        {
            // Confirmation query.
            if ((ec = query.block_confirmable(link)))
            {
                // Database errors are fatal.
                if (database::error::error_category::contains(ec))
                {
                    LOGF("Confirm [" << height << "] " << ec.message());
                    fault(ec);
                    return;
                }

                if (!query.set_unstrong(link))
                {
                    fault(error::confirm8);
                    return;
                }

                if (!query.set_block_unconfirmable(link))
                {
                    fault(error::confirm9);
                    return;
                }

                if (!roll_back(popped, fork_point, sub1(height)))
                    fault(error::confirm10);

                // UNCONFIRMABLE BLOCK (NON FAULT)
                notify(ec, chase::unconfirmable, link);
                fire(events::block_unconfirmable, height);
                LOGR("Unconfirmable block [" << height << "] " << ec.message());
                return;
            }

            // Set before changing block state.
            if (!query.set_filter_head(link))
            {
                fault(error::confirm11);
                return;
            }

            // Prevents reconfirm of entire chain.
            if (!query.set_block_confirmable(link))
            {
                fault(error::confirm12);
                return;
            }

            // CONFIRMABLE BLOCK
            notify(error::success, chase::confirmable, height);
            fire(events::block_confirmed, height);
            LOGV("Block confirmable: " << height);
        }
        else
        {
            ///////////////////////////////////////////////////////////////////
            // BUGBUG: A candidate organization race can cause this.
            ///////////////////////////////////////////////////////////////////
            fault(error::confirm13);
            return;
        }

        // Set strong (if not bypass) and push to confirmed index.
        // This must not preceed set_block_confirmable (double spend).
        if (!set_organized(link, height, bypass))
        {
            fault(error::confirm14);
            return;
        }

        fork.pop_back();
        set_position(height++);
    }
}

// private setters
// ----------------------------------------------------------------------------
// These affect only confirmed chain and strong tx state (not candidate).

///////////////////////////////////////////////////////////////////////////////
// BUGBUG: reorganize/organize operations that span a disk full recovery
// BUGBUG: may be inconsistent between set/unset_strong and push/pop_candidate.
///////////////////////////////////////////////////////////////////////////////

// Milestoned blocks can become formerly-confirmed.
// Checkpointed blocks cannot become formerly-confirmed.
// Reorganization sets unstrong on any formerly-confirmed blocks.
bool chaser_confirm::set_reorganized(const header_link& link,
    height_t confirmed_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // TODO: disk full race.
    if (!query.set_unstrong(link) || !query.pop_confirmed())
        return false;

    notify(error::success, chase::reorganized, link);
    fire(events::block_reorganized, confirmed_height);
    LOGV("Block reorganized: " << confirmed_height);
    return true;
}

// Milestoned (bypassed) blocks are set strong by organizer.
// Checkpointed (bypassed) blocks are set strong by archiver.
// Organization sets strong on newly-confirmed non-bypassed blocks.
bool chaser_confirm::set_organized(const header_link& link,
    height_t confirmed_height, bool bypassed) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // TODO: disk full race.
    if ((!bypassed && !query.set_strong(link)) || !query.push_confirmed(link))
        return false;

    notify(error::success, chase::organized, link);
    fire(events::block_organized, confirmed_height);
    LOGV("Block organized: " << confirmed_height);
    return true;
}

// Rollback to the fork point, then forward through previously popped.
// Rollback cannot apply to bypassed blocks, so always set/unset strong.
bool chaser_confirm::roll_back(const header_links& popped, size_t fork_point,
    size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    for (auto height = top; height > fork_point; --height)
        if (!set_reorganized(query.to_confirmed(height), height))
            return false;

    for (const auto& fk: std::views::reverse(popped))
        if (!set_organized(fk, ++fork_point, true))
            return false;

    return true;
}

// private getters
// ----------------------------------------------------------------------------
// These are subject to intervening/concurrent candidate chain reorganization.

bool chaser_confirm::get_fork_work(uint256_t& fork_work, header_links& fork,
    height_t fork_top) const NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    header_link link{};
    fork_work = zero;
    fork.clear();

    // Walk down candidates from fork_top to fork point (highest common).
    for (link = query.to_candidate(fork_top); !link.is_terminal() &&
        !query.is_confirmed_block(link); link = query.to_candidate(--fork_top))
    {
        uint32_t bits{};
        if (!query.get_bits(bits, link))
            return false;

        fork_work += chain::header::proof(bits);
        fork.push_back(link);
    }

    // Terminal candidate from previously valid height implies regression.
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
    BC_ASSERT(stranded());
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
