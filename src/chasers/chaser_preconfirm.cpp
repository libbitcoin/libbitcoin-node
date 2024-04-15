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
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_preconfirm::chaser_preconfirm(full_node& node) NOEXCEPT
  : chaser(node),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_blocks_(node.config().bitcoin.subsidy_interval_blocks)
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
    switch (event_)
    {
        case chase::start:
        {
            POST(do_checked, height_t{});
            break;
        }
        case chase::header:
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
        ////case chase::header:
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

    // Validate checked blocks.
    for (auto height = add1(last_); !closed(); ++height)
    {
        // Precondition (associated).
        // ....................................................................

        const auto link = query.to_candidate(height);
        if (!query.is_associated(link))
            return;

        // Accept/Connect block.
        // ....................................................................

        if (const auto code = validate(link, height))
        {
            if (code == error::validation_bypass ||
                code == database::error::block_confirmable ||
                code == database::error::block_preconfirmable)
            {
                // Advance.
                ++last_;
                notify(code, chase::preconfirmable, height);
                fire(events::validate_bypassed, height);
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

                notify(code, chase::unpreconfirmable, link);
                fire(events::block_unconfirmable, height);
            }

            LOGR("Unpreconfirmed block [" << height << "] " << code.message());
            return;
        }

        // Commit validation metadata.
        // ....................................................................

        // [set_txs_connected] FOR PERFORMANCE EVALUATION ONLY.
        // Tx validation/states are independent of block validation.
        if (!query.set_txs_connected(link) ||
            !query.set_block_preconfirmable(link))
        {
            fault(error::store_integrity);
            return;
        }

        // Advance.
        // ....................................................................

        ++last_;
        notify(error::success, chase::preconfirmable, height);
        fire(events::block_validated, height);
    }
}

code chaser_preconfirm::validate(const header_link& link,
    size_t height) const NOEXCEPT
{
    const auto& query = archive();

    if (is_under_bypass(height) && !query.is_malleable(link))
        return error::validation_bypass;

    auto ec = query.get_block_state(link);
    if (ec == database::error::block_confirmable ||
        ec == database::error::block_unconfirmable ||
        ec == database::error::block_preconfirmable)
        return ec;

    database::context context{};
    const auto block_ptr = query.get_block(link);
    if (!block_ptr || !query.get_context(context, link))
        return error::store_integrity;

    const auto& block = *block_ptr;
    if (!query.populate(block))
        return system::error::missing_previous_output;

    const chain::context ctx
    {
        context.flags,  // [accept & connect]
        {},             // timestamp
        {},             // mtp
        context.height, // [accept]
        {},             // minimum_block_version
        {}              // work_required
    };

    return ec = block.accept(ctx, subsidy_interval_blocks_, initial_subsidy_) ?
        ec : block.connect(ctx);
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
