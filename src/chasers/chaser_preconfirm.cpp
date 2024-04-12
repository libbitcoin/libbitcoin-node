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
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_blocks_(node.config().bitcoin.subsidy_interval_blocks),
    checkpoints_(node.config().bitcoin.checkpoints)
{
}

code chaser_preconfirm::start() NOEXCEPT
{
    last_ = archive().get_fork();
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

void chaser_preconfirm::handle_event(const code&, chase event_,
    event_link value) NOEXCEPT
{
    // These come out of order, advance in order asynchronously.
    // Asynchronous completion results in out of order notification.
    using namespace system;
    switch (event_)
    {
        case chase::start:
        {
            POST(do_checked, height_t{});
            break;
        }
        case chase::checked:
        {
            POST(do_height_checked, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_disorganized, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::stop:
        {
            // TODO: handle fault.
            break;
        }
        ////case chase::start:
        case chase::pause:
        case chase::resume:
        case chase::starved:
        case chase::split:
        case chase::stall:
        case chase::purge:
        case chase::block:
        case chase::header:
        case chase::download:
        ////case chase::checked:
        case chase::unchecked:
        case chase::preconfirmable:
        case chase::unpreconfirmable:
        case chase::confirmable:
        case chase::unconfirmable:
        case chase::organized:
        case chase::reorganized:
        ////case chase::disorganized:
        case chase::malleated:
        case chase::transaction:
        case chase::template_:
        ////case chase::stop:
        {
            break;
        }
    }
}

void chaser_preconfirm::do_disorganized(height_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    last_ = top;
    do_checked(top);
}

void chaser_preconfirm::do_height_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (height == add1(last_))
        do_checked(height);
}

void chaser_preconfirm::do_checked(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    if (closed())
        return;

    for (auto height = add1(last_); !closed(); ++height)
    {
        const auto link = query.to_candidate(height);
        if (!query.is_associated(link))
            return;

        // Always validate malleable blocks.
        // Only coincident malleability is possible here.
        if ((checkpoint::is_under(checkpoints_, height) ||
            is_under_milestone(height)) && !query.is_malleable(link))
        {
            ++last_;
            notify(error::validation_bypass, chase::preconfirmable, height);
            fire(events::block_bypassed, height);
            continue;
        }

        ////// This optimization is probably not worth the query cost.
        ////// Maybe in the case of a resart with long candidate branch.
        ////auto ec = query.get_block_state(link);
        ////if (ec == database::error::block_confirmable ||
        ////    ec == database::error::block_preconfirmable)
        ////{
        ////    ++last_;
        ////    notify(ec, chase::preconfirmable, height);
        ////    LOGN("Preconfirmed [" << height << "] " << ec.message());
        ////    fire(events::block_validated, height);
        ////    continue;
        ////}

        ////// This is probably dead code as header chain must catch.
        ////if (ec == database::error::block_unconfirmable)
        ////{
        ////    notify(ec, chase::unpreconfirmable, link);
        ////    LOGN("Unpreconfirmed [" << height << "] " << ec.message());
        ////    return;
        ////}

        // Reconstruct block for accept/connect.
        database::context ctx{};
        const auto block_ptr = query.get_block(link);
        if (!block_ptr || !query.get_context(ctx, link))
        {
            fault(error::store_integrity);
            return;
        }

        const auto& block = *block_ptr;

        // Accept/Connect block.
        // --------------------------------------------------------------------

        if (const auto error = validate(block, ctx))
        {
            // Only coincident malleability is possible here.
            if (block.is_malleable())
            {
                notify(error, chase::malleated, link);
            }
            else
            {
                if (!query.set_block_unconfirmable(link))
                {
                    fault(node::error::store_integrity);
                    return;
                }

                notify(error, chase::unpreconfirmable, link);
                fire(events::block_unconfirmable, height);
            }

            LOGR("Unpreconfirmed block [" << encode_hash(block.hash()) << ":" 
                << ctx.height << "] " << error.message());
            return;
        }

        // Commit validation metadata.
        // --------------------------------------------------------------------

        // [set_txs_connected] FOR PERFORMANCE EVALUATION ONLY.
        // Tx validation/states are independent of block validation.
        if (!query.set_txs_connected(link) ||
            !query.set_block_preconfirmable(link))
        {
            fault(error::store_integrity);
            return;
        }

        // --------------------------------------------------------------------

        ++last_;
        notify(error::success, chase::preconfirmable, height);
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
