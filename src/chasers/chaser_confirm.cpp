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

// Single higher priority thread strand (base class strand uses network pool).
// Higher priority than validator ensures locality to validator reads.
chaser_confirm::chaser_confirm(full_node& node) NOEXCEPT
  : chaser(node),
    concurrent_(node.config().node.concurrent_confirmation),
    threadpool_(one, node.config().node.priority_validation ?
        network::thread_priority::highest : network::thread_priority::high),
    strand_(threadpool_.service().get_executor())
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
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // TODO: resume needs to ensure a bump.
    // Stop generating query during suspension.
    if (suspended())
        return true;

    // An unconfirmable block height must not land here. 
    // These can come out of order, advance in order synchronously.
    switch (event_)
    {
        case chase::blocks:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));

            if (concurrent_ || mature_)
            {
                ////// TODO: value is branch point.
                ////POST(do_validated, std::get<height_t>(value));
                ////boost::asio::post(strand_,
                ////    BIND(do_validated, std::get<height_t>(value)));
            }

            break;
        }
        case chase::valid:
        {
            // value is validated block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));

            if (concurrent_ || mature_)
            {
                ////POST(do_validated, std::get<height_t>(value));
                boost::asio::post(strand_,
                    BIND(do_validated, std::get<height_t>(value)));
            }

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
// Blocks are either confirmed (blocks first) or validated/confirmed (headers
// first) here. Candidate chain reorganizations will result in reported heights
// moving in any direction. Each is treated as independent and only one
// representing a stronger chain is considered.

// Compute relative work, set fork and fork_point, and invoke reorganize.
void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    bool strong{};
    uint256_t work{};
    header_links fork{};

    // Scan from validated height to first confirmed, save links, and sum work.
    if (!get_fork_work(work, fork, height))
    {
        fault(error::get_fork_work);
        return;
    }

    // No longer a candidate fork (may have been reorganized).
    if (fork.empty())
        return;

    // fork_point is the highest candidate <-> confirmed common block.
    const auto fork_point = height - fork.size();
    if (!get_is_strong(strong, work, fork_point))
    {
        fault(error::get_is_strong);
        return;
    }

    // Fork is strong (has more work than confirmed branch).
    if (strong)
        do_reorganize(fork, fork_point);
}

// Pop confirmed chain from top down to above fork point, save popped.
void chaser_confirm::do_reorganize(header_links& fork, size_t fork_point) NOEXCEPT
{
    auto& query = archive();

    auto top = query.get_top_confirmed();
    if (top < fork_point)
    {
        fault(error::invalid_fork_point);
        return;
    }

    header_links popped{};
    while (top > fork_point)
    {
        const auto top_link = query.to_confirmed(top);
        if (top_link.is_terminal())
        {
            fault(error::to_confirmed);
            return;
        }

        popped.push_back(top_link);
        if (!query.set_unstrong(top_link))
        {
            fault(error::set_unstrong);
            return;
        }

        if (!query.pop_confirmed())
        {
            fault(error::pop_confirmed);
            return;
        }

        notify(error::success, chase::reorganized, top_link);
        fire(events::block_reorganized, top--);
    }

    // Top is now fork_point.
    do_organize(fork, popped, fork_point);
}

// Push candidate headers from above fork point to confirmed chain.
void chaser_confirm::do_organize(header_links& fork,
    const header_links& popped, size_t fork_point) NOEXCEPT
{
    auto& query = archive();

    // height tracks each fork link, starting at fork_point+1.
    size_t height = fork_point;

    while (!fork.empty())
    {
        ++height;
        const auto& link = fork.back();

        if (is_under_checkpoint(height) || query.is_milestone(link))
        {
            notify(error::success, chase::confirmable, height);
            fire(events::confirm_bypassed, height);
            LOGV("Block confirmation bypassed and organized: " << height);

            // Checkpoint and milestone are already set_strong.
            if (!set_organized(link, height))
            {
                fault(database::error::integrity);
                return;
            }
        }
        else
        {
            auto ec = query.get_block_state(link);
            if (ec == database::error::block_valid)
            {
                if (!query.set_strong(link))
                {
                    fault(database::error::integrity);
                    return;
                }
 
                // This spawns threads internally, blocking until complete.
                if ((ec = query.block_confirmable(link)))
                {
                    if (ec == database::error::integrity)
                    {
                        fault(database::error::integrity);
                        return;
                    }

                    notify(ec, chase::unconfirmable, link);
                    fire(events::block_unconfirmable, height);
                    LOGR("Unconfirmable block [" << height << "] "
                        << ec.message());

                    if (!query.set_unstrong(link))
                    {
                        fault(database::error::integrity);
                        return;
                    }

                    if (!query.set_block_unconfirmable(link))
                    {
                        fault(database::error::integrity);
                        return;
                    }

                    if (!roll_back(popped, fork_point, sub1(height)))
                        fault(database::error::integrity);

                    return;
                }

                // TODO: fees.
                if (!query.set_block_confirmable(link, {}))
                {
                    fault(database::error::integrity);
                    return;
                }

                notify(ec, chase::confirmable, height);
                fire(events::block_confirmed, height);
                LOGV("Block confirmed and organized: " << height);

                if (!set_organized(link, height))
                    fault(database::error::integrity);

                return;
            }
            else if (ec == database::error::block_confirmable)
            {
                if (!query.set_strong(link))
                {
                    fault(database::error::integrity);
                    return;
                }

                notify(error::success, chase::confirmable, height);
                fire(events::confirm_bypassed, height);
                LOGV("Block previously confirmable organized: " << height);

                if (!set_organized(link, height))
                    fault(database::error::integrity);

                return;
            }
            else if (ec == database::error::block_unconfirmable)
            {
                notify(ec, chase::unconfirmable, link);
                fire(events::block_unconfirmable, height);
                LOGV("Block confirmation failed: " << height);

                if (!roll_back(popped, fork_point, sub1(height)))
                    fault(database::error::integrity);

                return;
            }
            else
            {
                // With or without an error code, shouldn't be here.
                // database::error::block_valid         [canonical state  ]
                // database::error::block_confirmable   [resurrected state]
                // database::error::block_unconfirmable [shouldn't be here] ?
                // database::error::unknown_state       [shouldn't be here]
                // database::error::unassociated        [shouldn't be here]
                // database::error::unvalidated         [shouldn't be here]
                fault(database::error::integrity);
                return;
            }
        }

        fork.pop_back();
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

// Roll back to the specified new top. Header organization may then return the
// node to a previously-stronger branch - no need to do that here.
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
