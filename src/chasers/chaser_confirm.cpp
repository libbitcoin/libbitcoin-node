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
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_confirm::chaser_confirm(full_node& node) NOEXCEPT
  : chaser(node),
    filter_(node.archive().filter_enabled())
{
}

code chaser_confirm::start() NOEXCEPT
{
    const auto& query = archive();
    set_position(query.get_fork());

    if ((recent_ = is_recent()))
    {
        LOGN("Node is recent at startup block [" << position() << "].");
    }

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
        case chase::resume:
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

// Track validation
// ----------------------------------------------------------------------------

void chaser_confirm::do_regressed(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
}

void chaser_confirm::do_validated(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    do_bumped({});
}

void chaser_confirm::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    do_bumped({});
}

// Confirm (not cancellable)
// ----------------------------------------------------------------------------

// Compute relative work, set fork and fork_point, and invoke reorganize.
void chaser_confirm::do_bumped(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (closed())
        return;

    // Guarded by candidate interlock.
    size_t fork_point{};
    auto fork = query.get_validated_fork(fork_point, checkpoint(), filter_);

    // Fork may be empty if candidates were reorganized.
    if (fork.empty())
        return;

    // Cannot be forking above top.
    const auto top = query.get_top_confirmed();
    if (fork_point > top)
    {
        fault(error::confirm1);
        return;
    }

    // Compare work if fork is below top.
    if (fork_point < top)
    {
        // Gets work of candidate branch (above fork point).
        uint256_t work{};
        if (!query.get_work(work, fork))
        {
            fault(error::confirm2);
            return;
        }

        // Compares candidate branch work to confirmed (above fork point).
        bool strong{};
        if (!query.get_strong_fork(strong, work, fork_point))
        {
            fault(error::confirm3);
            return;
        }

        // Fork does not have more work than confirmed branch. Position moves
        // up to accumulate blocks until sufficient work, or regression resets.
        if (!strong)
            return;
    }

    reorganize(fork, top, fork_point);
}

// Pop confirmed chain from top down to above fork point, save popped.
void chaser_confirm::reorganize(header_states& fork, size_t top,
    size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    header_links popped{};

    while (top > fork_point)
    {
        const auto link = query.to_confirmed(top);
        if (link.is_terminal())
        {
            fault(error::confirm4);
            return;
        }

        popped.push_back(link);
        if (!set_reorganized(link, top--))
        {
            fault(error::confirm5);
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
void chaser_confirm::organize(header_states& fork, const header_links& popped,
    size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();
    auto height = add1(fork_point);

    for (const auto& state: fork)
    {
        switch (state.ec.value())
        {
            case database::error::bypassed:
            {
                if (!query.set_filter_head(state.link))
                {
                    fault(error::confirm6);
                    return;
                }

                complete_block(error::success, state.link, height, true);
                break;
            }
            case database::error::block_valid:
            {
                if (!confirm_block(state.link, height, popped, fork_point))
                    return;

                break;
            }
            case database::error::block_confirmable:
            {
                // Previously confirmable is not considered bypass.
                complete_block(error::success, state.link, height, false);
                break;
            }
            // error::unassociated
            // error::unknown_state
            // error::block_unconfirmable
            default:
            {
                fault(error::confirm7);
                return;
            }
        }

        // After set_block_confirmable.
        if (!set_organized(state.link, height++))
        {
            fault(error::confirm8);
            return;
        }
    }

    // Prevent stall by posting internal event, avoiding external handlers.
    // Posts new work, preventing recursion and releasing reorganization lock.
    handle_event(error::success, chase::bump, height_t{});
}

bool chaser_confirm::confirm_block(const header_link& link,
    size_t height, const header_links& popped, size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    if (const auto ec = query.block_confirmable(link))
    {
        if (!query.set_unstrong(link))
        {
            fault(error::confirm9);
            return false;
        }

        if (!query.set_block_unconfirmable(link))
        {
            fault(error::confirm10);
            return false;
        }

        if (!roll_back(popped, fork_point, sub1(height)))
        {
            fault(error::confirm11);
            return false;
        }

        return complete_block(ec, link, height, false);
    }

    // Before set_block_confirmable.
    if (!query.set_filter_head(link))
    {
        fault(error::confirm12);
        return false;
    }

    if (!query.set_block_confirmable(link))
    {
        fault(error::confirm13);
        return false;
    }

    return complete_block(error::success, link, height, false);
}

bool chaser_confirm::complete_block(const code& ec, const header_link& link,
    size_t height, bool /* bypass */) NOEXCEPT
{
    if (ec)
    {
        // Database errors are fatal.
        if (database::error::error_category::contains(ec))
        {
            LOGF("Fault confirming [" << height << "] " << ec.message());
            fault(ec);
            return false;
        }

        // UNCONFIRMABLE BLOCK (not a fault but discontinue)
        notify(ec, chase::unconfirmable, link);
        fire(events::block_unconfirmable, height);
        LOGR("Unconfirmable block [" << height << "] " << ec.message());
        return false;
    }

    // CONFIRMABLE BLOCK (bypass could be reported)
    notify(error::success, chase::confirmable, height);
    fire(events::block_confirmed, height);
    LOGV("Block confirmable: " << height);
    return true;
}

// private
// ----------------------------------------------------------------------------
// Checkpointed blocks are set strong by archiver, and cannot be reorganized.

bool chaser_confirm::set_reorganized(const header_link& link,
    height_t confirmed_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT(!is_under_checkpoint(confirmed_height));
    if (!archive().pop_confirmed())
        return false;

    notify(error::success, chase::reorganized, link);
    fire(events::block_reorganized, confirmed_height);
    LOGV("Block reorganized: " << confirmed_height);
    return true;
}

bool chaser_confirm::set_organized(const header_link& link,
    height_t confirmed_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

#if !defined(NDEBUG)
    const auto previous_height = query.get_top_confirmed();
    if (confirmed_height != add1(previous_height))
    {
        fault(error::suspended_channel);
        return false;
    }

    const auto parent = query.to_parent(link);
    const auto top = query.to_confirmed(previous_height);
    if (parent != top)
    {
        fault(error::suspended_service);
        return false;
    }
#endif // !NDEBUG

    // Checkpointed blocks are set strong by archiver.
    if (!query.push_confirmed(link, !is_under_checkpoint(confirmed_height)))
        return false;

    notify(error::success, chase::organized, link);
    fire(events::block_organized, confirmed_height);
    LOGV("Block organized: " << confirmed_height);

    // Announce newly-organized block.
    if (is_current(true))
        notify(error::success, chase::block, link);

    // When current or maximum height take snapshot, which resets connections.
    // Node may fall behind after becomming current though this does not reset.
    if (!recent_ && is_recent())
    {
        recent_ = true;
        notify(error::success, chase::snap, confirmed_height);
        LOGN("Node is recent as of block [" << confirmed_height << "].");
    }

    return true;
}

// Rollback to the fork point, then forward through previously popped.
bool chaser_confirm::roll_back(const header_links& popped, size_t fork_point,
    size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    for (auto height = top; height > fork_point; --height)
        if (!set_reorganized(query.to_confirmed(height), height))
            return false;

    for (const auto& fk: std::views::reverse(popped))
        if (!set_organized(fk, ++fork_point))
            return false;

    return true;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
