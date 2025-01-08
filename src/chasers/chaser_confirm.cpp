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
    threadpool_(one, node.config().node.priority_()),
    independent_strand_(threadpool_.service().get_executor()),
    concurrent_(node.config().node.concurrent_confirmation)
{
}

code chaser_confirm::start() NOEXCEPT
{
    const auto& query = archive();
    filters_ = query.neutrino_enabled();
    reset_position(query.get_fork());
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
    // These come out of order, advance in order synchronously.
    switch (event_)
    {
        ////case chase::blocks:
        ////{
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////
        ////    if (concurrent_ || mature_)
        ////    {
        ////        // TODO: value is branch point.
        ////        POST(do_validated, std::get<height_t>(value));
        ////    }
        ////
        ////    break;
        ////}
        case chase::start:
        case chase::bump:
        {
            if (concurrent_ || mature_)
            {
                POST(do_bump, height_t{});
            }

            break;
        }
        case chase::valid:
        {
            // value is validated block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));

            if (concurrent_ || mature_)
            {
                POST(do_validated, std::get<height_t>(value));
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

// track validation
// ----------------------------------------------------------------------------

void chaser_confirm::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    for (auto height = position(); height > branch_point; --height)
    {
        if (!query.set_unstrong(query.to_candidate(height)))
        {
            fault(error::confirm1);
            return;
        }
    }

    reset_position(branch_point);
}

// Candidate block validated at given height, if next then confirm/advance.
void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (height == add1(position()))
        do_bump(height);
}

// TODO: This is simplified single thread variant of full implementation below.
// This variant doesn't implement the relative work check and instead confirms
// one block at a time, just like validation.
void chaser_confirm::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Keep working past window if possible.
    for (auto height = add1(position()); !closed(); ++height)
    {
        const auto link = query.to_candidate(height);
        auto ec = query.get_block_state(link);

        // Don't report bypassed block is confirmable until associated.
        if (ec == database::error::unassociated)
            return;

        if (is_under_checkpoint(height) || query.is_milestone(link))
        {
            notify(error::success, chase::confirmable, height);
            fire(events::confirm_bypassed, height);
            LOGV("Block confirmation bypassed: " << height);
            ////return;
        }
        else if (ec == database::error::block_valid)
        {
            if (!query.set_strong(link))
            {
                fault(error::confirm2);
                return;
            }

            //////////////////////////////////////////
            // Confirmation query.
            // This will pull from new prevouts table.
            //////////////////////////////////////////
            if ((ec = query.block_confirmable(link)))
            {
                if (ec == database::error::integrity)
                {
                    fault(error::confirm3);
                    return;
                }

                if (!query.set_block_unconfirmable(link))
                {
                    fault(error::confirm4);
                    return;
                }

                if (!query.set_unstrong(link))
                {
                    fault(error::confirm5);
                    return;
                }

                // Blocks between link and fork point will be set_unstrong
                // by header reorganization, picked up by do_regressed.
                notify(ec, chase::unconfirmable, link);
                fire(events::block_unconfirmable, height);
                LOGR("Unconfirmable block [" << height << "] "
                    << ec.message());
                return;
            }

            if (!query.set_block_confirmable(link))
            {
                fault(error::confirm6);
                return;
            }

            if (!set_organized(link, height))
            {
                fault(error::confirm7);
                return;
            }
        
            notify(error::success, chase::confirmable, height);
            fire(events::block_confirmed, height);
            LOGV("Block confirmed: " << height);
            ////return;
        }
        else if (ec == database::error::block_confirmable)
        {
            if (!query.set_strong(link))
            {
                fault(error::confirm8);
                return;
            }
        
            notify(error::success, chase::confirmable, height);
            fire(events::confirm_bypassed, height);
            LOGV("Block previously confirmable: " << height);
            ////return;
        }
        else
        {
            // With or without an error code, shouldn't be here.
            // database::error::block_valid         [canonical state  ]
            // database::error::block_confirmable   [resurrected state]
            // database::error::block_unconfirmable [shouldn't be here]
            // database::error::unknown_state       [shouldn't be here]
            // database::error::unassociated        [shouldn't be here]
            // database::error::unvalidated         [shouldn't be here]
            fault(error::confirm9);
            return;
        }

        if (!update_neutrino(link))
        {
            fault(error::confirm10);
            return;
        }

        set_position(height);
    }
}

// confirm
// ----------------------------------------------------------------------------
// Blocks are either confirmed (blocks first) or validated/confirmed (headers
// first) here. Candidate chain reorganizations will result in reported heights
// moving in any direction. Each is treated as independent and only one
// representing a stronger chain is considered.
////
////// Compute relative work, set fork and fork_point, and invoke reorganize.
////void chaser_confirm::do_validated(height_t height) NOEXCEPT
////{
////    BC_ASSERT(stranded());
////
////    if (closed())
////        return;
////
////    bool strong{};
////    uint256_t work{};
////    header_links fork{};
////
////    // Scan from validated height to first confirmed, save links, and sum work.
////    if (!get_fork_work(work, fork, height))
////    {
////        fault(error::confirm1);
////        return;
////    }
////
////    // No longer a candidate fork (may have been reorganized).
////    if (fork.empty())
////        return;
////
////    // fork_point is the highest candidate <-> confirmed common block.
////    const auto fork_point = height - fork.size();
////    if (!get_is_strong(strong, work, fork_point))
////    {
////        fault(error::confirm2);
////        return;
////    }
////
////    // Fork is strong (has more work than confirmed branch).
////    if (strong)
////        do_reorganize(fork, fork_point);
////}
////
////// Pop confirmed chain from top down to above fork point, save popped.
////void chaser_confirm::do_reorganize(header_links& fork, size_t fork_point) NOEXCEPT
////{
////    auto& query = archive();
////
////    auto top = query.get_top_confirmed();
////    if (top < fork_point)
////    {
////        fault(error::confirm3);
////        return;
////    }
////
////    header_links popped{};
////    while (top > fork_point)
////    {
////        const auto top_link = query.to_confirmed(top);
////        if (top_link.is_terminal())
////        {
////            fault(error::confirm4);
////            return;
////        }
////
////        popped.push_back(top_link);
////        if (!query.set_unstrong(top_link))
////        {
////            fault(error::confirm5);
////            return;
////        }
////
////        if (!query.pop_confirmed())
////        {
////            fault(error::confirm6);
////            return;
////        }
////
////        notify(error::success, chase::reorganized, top_link);
////        fire(events::block_reorganized, top--);
////    }
////
////    // Top is now fork_point.
////    do_organize(fork, popped, fork_point);
////}
////
////// Push candidate headers from above fork point to confirmed chain.
////void chaser_confirm::do_organize(header_links& fork,
////    const header_links& popped, size_t fork_point) NOEXCEPT
////{
////    auto& query = archive();
////
////    // height tracks each fork link, starting at fork_point+1.
////    auto height = fork_point;
////
////    while (!fork.empty())
////    {
////        ++height;
////        const auto& link = fork.back();
////
////        if (is_under_checkpoint(height) || query.is_milestone(link))
////        {
////            notify(error::success, chase::confirmable, height);
////            fire(events::confirm_bypassed, height);
////            LOGV("Block confirmation bypassed and organized: " << height);
////
////            // Checkpoint and milestone are already set_strong.
////            if (!set_organized(link, height))
////            {
////                fault(error::confirm7);
////                return;
////            }
////        }
////        else
////        {
////            auto ec = query.get_block_state(link);
////            if (ec == database::error::block_valid)
////            {
////                if (!query.set_strong(link))
////                {
////                    fault(error::confirm8);
////                    return;
////                }
//// 
////                // This spawns threads internally, blocking until complete.
////                if ((ec = query.block_confirmable(link)))
////                {
////                    if (ec == database::error::integrity)
////                    {
////                        fault(error::confirm9);
////                        return;
////                    }
////
////                    notify(ec, chase::unconfirmable, link);
////                    fire(events::block_unconfirmable, height);
////                    LOGR("Unconfirmable block [" << height << "] "
////                        << ec.message());
////
////                    if (!query.set_unstrong(link))
////                    {
////                        fault(error::confirm10);
////                        return;
////                    }
////
////                    if (!query.set_block_unconfirmable(link))
////                    {
////                        fault(error::confirm11);
////                        return;
////                    }
////
////                    if (!roll_back(popped, fork_point, sub1(height)))
////                        fault(error::confirm12);
////
////                    return;
////                }
////
////                // TODO: fees.
////                if (!query.set_block_confirmable(link, {}))
////                {
////                    fault(error::confirm13);
////                    return;
////                }
////
////                notify(ec, chase::confirmable, height);
////                fire(events::block_confirmed, height);
////                LOGV("Block confirmed and organized: " << height);
////
////                if (!set_organized(link, height))
////                    fault(error::confirm14);
////
////                return;
////            }
////            else if (ec == database::error::block_confirmable)
////            {
////                if (!query.set_strong(link))
////                {
////                    fault(error::confirm15);
////                    return;
////                }
////
////                notify(error::success, chase::confirmable, height);
////                fire(events::confirm_bypassed, height);
////                LOGV("Block previously confirmable organized: " << height);
////
////                if (!set_organized(link, height))
////                    fault(error::confirm16);
////
////                return;
////            }
////            else
////            {
////                ///////////////////////////////////////////////////////////////
////                // TODO: do not start branch unless block valid or confirmable.
////                ///////////////////////////////////////////////////////////////
////
////                // With or without an error code, shouldn't be here.
////                // database::error::block_valid         [canonical state  ]
////                // database::error::block_confirmable   [resurrected state]
////                // database::error::block_unconfirmable [shouldn't be here]
////                // database::error::unknown_state       [shouldn't be here]
////                // database::error::unassociated        [shouldn't be here]
////                // database::error::unvalidated         [shouldn't be here]
////                ////fault(error::confirm17);
////                return;
////            }
////        }
////
////        fork.pop_back();
////    }
////    
////    ///////////////////////////////////////////////////////////////////////////
////    // TODO: terminal stall will happen here as validation completes.
////    ///////////////////////////////////////////////////////////////////////////
////}

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

// neutrino
// ----------------------------------------------------------------------------

bool chaser_confirm::update_neutrino(const header_link& link) NOEXCEPT
{
    // neutrino_.link is only used for this assertion, should compile away.
    BC_ASSERT(archive().get_height(link) == 
        add1(archive().get_height(neutrino_.link)));

    if (!filters_)
        return true;

    data_chunk filter{};
    auto& query = archive();
    if (!query.get_filter_body(filter, link))
        return false;

    neutrino_.link = link;
    neutrino_.head = neutrino::compute_filter_header(neutrino_.head, filter);
    return query.set_filter_head(link, neutrino_.head);
}

// Expects confirmed height.
// Use for startup and regression, to read current filter header from store.
void chaser_confirm::reset_position(size_t confirmed_height) NOEXCEPT
{
    set_position(confirmed_height);

    if (filters_)
    {
        const auto& query = archive();
        neutrino_.link = query.to_confirmed(confirmed_height);
        query.get_filter_head(neutrino_.head, neutrino_.link);
    }
}

// Strand.
// ----------------------------------------------------------------------------

network::asio::strand& chaser_confirm::strand() NOEXCEPT
{
    return independent_strand_;
}

bool chaser_confirm::stranded() const NOEXCEPT
{
    return independent_strand_.running_in_this_thread();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
