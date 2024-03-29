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
#include <bitcoin/node/chasers/chaser_preconfirm.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_preconfirm

using namespace system;
using namespace system::chain;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_preconfirm::chaser_preconfirm(full_node& node) NOEXCEPT
  : chaser(node),
    milestone_(node.config().bitcoin.milestone),
    checkpoints_(node.config().bitcoin.checkpoints),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_blocks_(node.config().bitcoin.subsidy_interval_blocks)
{
}

code chaser_preconfirm::start() NOEXCEPT
{
    last_ = archive().get_fork();
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// TODO: the existence of milestone should be cached/updated.
bool chaser_preconfirm::is_under_milestone(size_t height) const NOEXCEPT
{
    // get_header_key returns null_hash on not found.
    if (height > milestone_.height() || milestone_.hash() == null_hash)
        return false;

    const auto& query = archive();
    const auto link = query.to_candidate(milestone_.height());
    return query.get_header_key(link) == milestone_.hash();
}

void chaser_preconfirm::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    // These come out of order, advance in order asynchronously.
    // Asynchronous completion results in out of order notification.
    using namespace system;
    switch (event_)
    {
        case chase::bump:
        {
            POST(do_checked, size_t{});
            break;
        }
        case chase::checked:
        {
            POST(do_height_checked, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_disorganized, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::header:
        case chase::download:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::pause:
        case chase::resume:
        ////case chase::bump:
        ////case chase::checked:
        case chase::unchecked:
        case chase::preconfirmed:
        case chase::unpreconfirmed:
        case chase::confirmed:
        case chase::unconfirmed:
        ////case chase::disorganized:
        case chase::transaction:
        case chase::template_:
        case chase::block:
        case chase::stop:
        {
            break;
        }
    }
}

void chaser_preconfirm::do_disorganized(size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    last_ = top;

    // Assure consistency given chance of intervening candidate organization.
    do_checked(top);
}

void chaser_preconfirm::do_height_checked(size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (height == add1(last_))
        do_checked(height);
}

void chaser_preconfirm::do_checked(size_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (auto height = add1(last_); !closed(); ++height)
    {
        const auto link = query.to_candidate(height);
        if (!query.is_associated(link))
            return;

        if (checkpoint::is_under(checkpoints_, height))
        {
            ++last_;
            notify(error::checkpoint_bypass, chase::preconfirmed, height);
            fire(events::block_bypassed, height);
            continue;
        }

        if (is_under_milestone(height))
        {
            ++last_;
            notify(error::milestone_bypass, chase::preconfirmed, height);
            fire(events::block_bypassed, height);
            continue;
        }

        ////// This optimization is probably not worth the query cost.
        ////auto ec = query.get_block_state(link);
        ////if (ec == database::error::block_confirmable ||
        ////    ec == database::error::block_preconfirmable)
        ////{
        ////    ++last_;
        ////    notify(ec, chase::preconfirmed, height);
        ////    LOGN("Preconfirmed [" << height << "] " << ec.message());
        ////    fire(events::block_validated, height);
        ////    continue;
        ////}

        ////// This is probably dead code as header chain must catch.
        ////if (ec == database::error::block_unconfirmable)
        ////{
        ////    notify(ec, chase::unpreconfirmed, link);
        ////    LOGN("Unpreconfirmed [" << height << "] " << ec.message());
        ////    return;
        ////}

        database::context ctx{};
        const auto block = query.get_block(link);
        if (!block || !query.get_context(ctx, link))
        {
            close(error::store_integrity);
            return;
        }

        code ec{};
        if ((ec = validate(*block, ctx)))
        {
            // Do not set block_unconfirmable if its identifier is malleable.
            const auto malleable = block->is_malleable();
            if (!malleable && !query.set_block_unconfirmable(link))
            {
                close(error::store_integrity);
                return;
            }

            notify(ec, chase::unpreconfirmed, link);

            LOGN("Unpreconfirmed [" << height << "] " << ec.message()
                << (malleable ? " [MALLEABLE]." : ""));
            return;
        }

        if (!query.set_block_confirmable(link, block->fees()))
        {
            close(error::store_integrity);
            return;
        }

        ++last_;
        notify(ec, chase::preconfirmed, height);
        fire(events::block_validated, height);
    }
}

// utility
// ----------------------------------------------------------------------------

code chaser_preconfirm::validate(const chain::block& block,
    const database::context& ctx) const NOEXCEPT
{
    const chain::context context
    {
        ctx.flags,  // [accept & connect]
        {},         // timestamp
        {},         // mtp
        ctx.height, // [accept]
        {},         // minimum_block_version
        {}          // work_required
    };

    // Assumes all preceding candidates are associated.
    code ec{ system::error::missing_previous_output };
    if (!archive().populate(block))
        return ec;

    if ((ec = block.accept(context, subsidy_interval_blocks_,
        initial_subsidy_)))
        return ec;

    return block.connect(context);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
