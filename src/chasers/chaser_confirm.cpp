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
  : chaser(node)
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
        case chase::block:
        {
            POST(do_preconfirmed, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::preconfirmable:
        {
            POST(do_preconfirmed, possible_narrow_cast<height_t>(value));
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

// Blocks are either confirmed (blocks first) or preconfirmed/confirmed
// (headers first) at this point. An unconfirmable block may not land here.
// Candidate chain reorganizations will result in reported heights moving
// in any direction. Each is treated as independent and only one representing
// a stronger chain is considered. Currently total work at a given block is not
// archived, so this organization (like in the organizer) requires scanning to
// the fork point from the block and to the top confirmed from the fork point.
// The scans are extremely fast and tiny in all typical scnearios, so it may
// not improve performance or be worth spending 32 bytes per header to store
// work, especially since individual header work is obtained from 4 bytes.
void chaser_confirm::do_preconfirmed(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
        return;

    // Compute relative work.
    // ........................................................................
    // A reorg race may have resulted in height not now being a candidate.

    bool strong{};
    uint256_t work{};
    header_links fork{};

    if (!get_fork_work(work, fork, height))
    {
        suspend_network(error::get_fork_work);
        return;
    }

    if (!get_is_strong(strong, work, height))
    {
        suspend_network(error::get_is_strong);
        return;
    }

    // Nothing to do. In this case candidate top is/was above height.
    if (!strong)
        return;

    // Reorganize confirmed chain.
    // ........................................................................

    auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto fork_point = height - fork.size();
    if (top < fork_point)
    {
        suspend_network(error::invalid_fork_point);
        return;
    }

    // Pop down to the fork point.
    auto index = top;
    header_links popped{};
    while (index > fork_point)
    {
        popped.push_back(query.to_confirmed(index));
        if (popped.back().is_terminal())
        {
            suspend_network(error::to_confirmed);
            return;
        }

        if (!query.pop_confirmed())
        {
            suspend_network(error::pop_confirmed);
            return;
        }

        notify(error::success, chase::reorganized, popped.back());
        fire(events::block_reorganized, index--);
    }

    // fork_point + 1
    ++index;

    // Push candidate headers to confirmed chain.
    for (const auto& link: views_reverse(fork))
    {
        // Precondition (established by fork construction above).
        // ....................................................................

        // Confirm block.
        // ....................................................................

        if (const auto code = confirm(link, index))
        {
            if (code == error::confirmation_bypass ||
                code == database::error::block_confirmable)
            {
                // Advance and confirm.
                notify(code, chase::confirmable, index);
                fire(events::confirm_bypassed, index);

                // chase::organized & events::block_organized
                if (!set_confirmed(link, index++))
                {
                    suspend_network(error::set_confirmed);
                    return;
                }

                continue;
            }

            if (code == error::store_integrity)
            {
                suspend_network(error::node_confirm);
                return;
            }
        
            if (query.is_malleable(link))
            {
                // Index will be reported multiple times when 'height' is above.
                notify(code, chase::malleated, link);
                fire(events::block_malleated, index);
            }
            else
            {
                if (code != database::error::block_unconfirmable &&
                    !query.set_block_unconfirmable(link))
                {
                    suspend_network(error::set_block_unconfirmable);
                    return;
                }

                // Index will be reportd multiple times when 'height' us above.
                notify(code, chase::unconfirmable, link);
                fire(events::block_unconfirmable, index);
            }

            // chase::reorganized & events::block_reorganized
            // chase::organized & events::block_organized
            if (!roll_back(popped, fork_point, sub1(index)))
            {
                suspend_network(error::node_roll_back);
                return;
            }
        
            LOGR("Unconfirmable block [" << index << "] " << code.message());
            return;
        }

        // Commit confirmation metadata.
        // ....................................................................

        // TODO: compute fees from validation records (optional metadata).
        if (!query.set_block_confirmable(link, uint64_t{}))
        {
            suspend_network(error::block_confirmable);
            return;
        }

        // Advance and confirm.
        // ....................................................................

        notify(error::success, chase::confirmable, index);
        fire(events::block_confirmed, index);

        // chase::organized & events::block_organized
        if (!set_confirmed(link, index++))
        {
            suspend_network(error::set_confirmed);
            return;
        }

        LOGV("Block confirmed: " << height);
    }
}

code chaser_confirm::confirm(const header_link& link, size_t height) NOEXCEPT
{
    auto& query = archive();

    // All blocks must be set_strong.
    if (!query.set_strong(link))
        return error::store_integrity;

    if (is_under_bypass(height) && !query.is_malleable(link))
        return error::confirmation_bypass;

    const auto ec = query.get_block_state(link);
    if (ec == database::error::block_confirmable ||
        ec == database::error::block_unconfirmable)
        return ec;

    if (ec == database::error::block_preconfirmable)
        return error::success;

    // Should not get here without a known block state.
    return error::store_integrity;
}

// Private
// ----------------------------------------------------------------------------

bool chaser_confirm::set_confirmed(header_t link, height_t height) NOEXCEPT
{
    auto& query = archive();
    if (!query.push_confirmed(link))
        return false;

    notify(error::success, chase::organized, link);
    fire(events::block_organized, height);
    return true;
}

bool chaser_confirm::set_unconfirmed(header_t link, height_t height) NOEXCEPT
{
    auto& query = archive();
    if (!query.set_unstrong(link) || !query.pop_confirmed())
        return false;

    notify(error::success, chase::reorganized, link);
    fire(events::block_reorganized, height);
    return true;
}

bool chaser_confirm::roll_back(const header_links& popped,
    size_t fork_point, size_t top) NOEXCEPT
{
    auto& query = archive();
    for (auto height = add1(fork_point); height <= top; ++height)
        if (!set_unconfirmed(query.to_candidate(height), height))
            return false;

    for (const auto& link: views_reverse(popped))
        if (!query.set_strong(link) || !set_confirmed(link, ++fork_point))
            return false;

    return true;
}

bool chaser_confirm::get_fork_work(uint256_t& fork_work,
    header_links& fork, height_t fork_top) const NOEXCEPT
{
    const auto& query = archive();
    for (auto link = query.to_candidate(fork_top);
        !query.is_confirmed_block(link);
        link = query.to_candidate(--fork_top))
    {
        uint32_t bits{};
        if (link.is_terminal() || !query.get_bits(bits, link))
            return false;

        fork.push_back(link);
        fork_work += system::chain::header::proof(bits);
    }

    return true;
}

// A forl with greater work will cause confirmed reorganization.
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
