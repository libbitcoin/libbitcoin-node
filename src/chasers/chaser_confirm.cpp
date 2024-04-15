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
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_confirm::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
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
            // TODO: handle fault.
            break;
        }
        case chase::start:
        case chase::pause:
        case chase::resume:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        ////case chase::block:
        case chase::header:
        case chase::download:
        case chase::checked:
        case chase::unchecked:
        ////case chase::preconfirmable:
        case chase::unpreconfirmable:
        case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        case chase::disorganized:
        case chase::malleated:
        case chase::transaction:
        case chase::template_:
        ////case chase::stop:
        {
            break;
        }
    }
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
    auto& query = archive();

    if (closed())
        return;

    // Compute relative work.
    // ........................................................................
    // A reorg race may have resulted in height not now being a candidate.

    uint256_t work{};
    header_links fork{};
    if (!get_fork_work(work, fork, height))
        return;

    bool strong{};
    if (!get_is_strong(strong, work, height))
    {
        fault(error::store_integrity);
        return;
    }

    // Nothing to do. In this case candidate top is/was above height.
    if (!strong)
        return;

    // Reorganize confirmed chain.
    // ........................................................................

    const auto fork_point = height - fork.size();
    const auto top = query.get_top_confirmed();
    if (top < fork_point)
    {
        fault(error::store_integrity);
        return;
    }

    // Pop down to the fork point.
    auto index = top;
    header_links popped{};
    while (index > fork_point)
    {
        const auto link = query.to_confirmed(index);
        popped.push_back(link);

        if (!query.pop_confirmed() || link.is_terminal())
        {
            fault(error::store_integrity);
            return;
        }

        notify(error::success, chase::reorganized, link);
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

                if (!set_confirmed(link, index++))
                {
                    fault(error::store_integrity);
                    return;
                }
                continue;
            }

            if (code == error::store_integrity)
            {
                fault(error::store_integrity);
                return;
            }

            if (query.is_malleable(link))
            {
                notify(code, chase::malleated, link);
            }
            else
            {
                if (code != database::error::block_unconfirmable &&
                    !query.set_block_unconfirmable(link))
                {
                    fault(error::store_integrity);
                    return;
                }

                notify(code, chase::unconfirmable, link);
                fire(events::block_unconfirmable, index);
            }

            if (!roll_back(popped, fork_point, sub1(index)))
            {
                fault(error::store_integrity);
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
            fault(error::store_integrity);
            return;
        }

        // Advance and confirm.
        // ....................................................................

        notify(error::success, chase::confirmable, index);
        fire(events::block_confirmable, index);

        if (!set_confirmed(link, index++))
        {
            fault(error::store_integrity);
            return;
        }
    }
}

code chaser_confirm::confirm(const header_link& link,
    size_t height) const NOEXCEPT
{
    const auto& query = archive();

    if (is_under_bypass(height) && !query.is_malleable(link))
        return error::confirmation_bypass;

    const auto ec = query.get_block_state(link);
    if (ec == database::error::block_confirmable ||
        ec == database::error::block_unconfirmable)
        return ec;

    if (ec == database::error::block_preconfirmable)
        return query.block_confirmable(link);

    // Should not get here without a known block state.
    return error::store_integrity;
}

// utility
// ----------------------------------------------------------------------------

bool chaser_confirm::set_confirmed(header_t link, height_t height) NOEXCEPT
{
    auto& query = archive();
    if (!query.push_confirmed(link) || !query.set_strong(link))
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

    for (const auto& link : views_reverse(popped))
        if (!set_confirmed(link, ++fork_point))
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

// ****************************************************************************
// CONSENSUS: fork with greater work causes confirmed reorganization.
// ****************************************************************************
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

} // namespace database
} // namespace libbitcoin
