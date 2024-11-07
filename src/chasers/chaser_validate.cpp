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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <functional>
#include <memory>
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::neutrino;
using namespace database;
using namespace std::placeholders;

// TODO: update specialized fault codes, reintegrate neutrino.

// Shared pointer is required to keep the race object alive in bind closure.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Multiple higher priority thread strand (base class strand uses network pool).
// Higher priority than downloader (net) ensures locality to downloader writes.
chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    prepopulate_(node.config().node.prepopulate),
    concurrent_(node.config().node.concurrent_validation),
    maximum_backlog_(node.config().node.maximum_concurrency_()),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_(node.config().bitcoin.subsidy_interval_blocks),
    threadpool_(std::max(node.config().node.threads, 1_u32),
        node.config().node.priority_validation ?
        network::thread_priority::high : network::thread_priority::normal),
    strand_(threadpool_.service().get_executor())
{
}

code chaser_validate::start() NOEXCEPT
{
    update_position(archive().get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

void chaser_validate::stopping(const code& ec) NOEXCEPT
{
    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_validate::stop() NOEXCEPT
{
    if (!threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

bool chaser_validate::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    if (suspended())
        return true;

    // These come out of order, advance in order asynchronously.
    // Asynchronous completion again results in out of order notification.
    switch (event_)
    {
        case chase::start:
        case chase::bump:
        {
            if (concurrent_ || mature_)
            {
                ////POST(do_bump, height_t{});
                boost::asio::post(strand_,
                    BIND(do_bump, height_t{}));
            }

            break;
        }
        case chase::checked:
        {
            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));

            if (concurrent_ || mature_)
            {
                ////POST(do_checked, std::get<height_t>(value));
                boost::asio::post(strand_,
                    BIND(do_checked, std::get<height_t>(value)));
            }

            break;
        }
        case chase::regressed:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            ////POST(do_regressed, std::get<height_t>(value));
            boost::asio::post(strand_,
                BIND(do_regressed, std::get<height_t>(value)));
            break;
        }
        case chase::disorganized:
        {
            BC_ASSERT(std::holds_alternative<height_t>(value));
            ////POST(do_regressed, std::get<height_t>(value));
            boost::asio::post(strand_,
                BIND(do_regressed, std::get<height_t>(value)));
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

// track downloaded in order (to validate)
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (branch_point >= position())
        return;

    // Update position and wait.
    update_position(branch_point);
}

void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Candidate block was checked at the given height, validate/advance.
    if (height == add1(position()))
        do_bump(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Bypass until next event if validation backlog is full.
    for (auto height = add1(position());
        (validation_backlog_ < maximum_backlog_) && !closed(); ++height)
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Wait until the gap is filled at a current height (or next top).
        if (ec == database::error::unassociated)
            return;

        // The size of the job is not relevant to the backlog cost.
        ++validation_backlog_;

        if (ec == database::error::block_unconfirmable)
        {
            complete_block(ec, link, height);
            return;
        }

        set_position(height);

        if ((ec == database::error::block_valid) ||
            (ec == database::error::block_confirmable) ||
            is_under_checkpoint(height) || query.is_milestone(link))
        {
            complete_block(error::success, link, height);
            continue;
        }

        // concurrent by block
        boost::asio::post(threadpool_.service(),
            BIND(validate_block, link));
    }
}

void chaser_validate::validate_block(const header_link& link) NOEXCEPT
{
    if (closed())
        return;

    auto& query = archive();
    const auto block = query.get_block(link);
    if (!block)
    {
        ////POST(complete_block, error::validate1, link, zero);
        boost::asio::post(strand_,
            BIND(complete_block, error::validate1, link, zero));
        return;
    }

    chain::context ctx{};
    if (!query.get_context(ctx, link))
    {
        ////POST(complete_block, error::validate2, link, zero);
        boost::asio::post(strand_,
            BIND(complete_block, error::validate2, link, zero));
        return;
    }

    // TODO: hardwire after performance evaluation with and without.
    if (prepopulate_)
        block->populate();

    if (!query.populate(*block))
    {
        // This could instead be a case of invalid milestone.
        ////POST(complete_block, error::validate3, link, ctx.height);
        boost::asio::post(strand_,
            BIND(complete_block, error::validate3, link, ctx.height));
        return;
    }

    code ec{};
    if (((ec = block->accept(ctx, subsidy_interval_, initial_subsidy_))) ||
        ((ec = block->connect(ctx))))
    {
        if (!query.set_block_unconfirmable(link))
        {
            ec = error::validate4;
        }
    }
    else if (!query.set_block_valid(link))
    {
        ec = error::validate5;
    }
    else
    {
        fire(events::block_validated, ctx.height);
    }

    ////POST(complete_block, ec, link, ctx.height);
    boost::asio::post(strand_,
        BIND(complete_block, ec, link, ctx.height));
}

void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // The size of the job is not relevant to the backlog cost.
    --validation_backlog_;

    if (ec)
    {
        if (ec == error::validate6)
        {
            fault(ec);
            return;
        }

        LOGR("Unconfirmable block [" << height << "] " << ec.message());
        fire(events::block_unconfirmable, height);
        notify(ec, chase::unvalid, link);
        return;
    }

    notify(ec, chase::valid, possible_wide_cast<height_t>(height));

    // Prevent stall by posting internal event, avoid hitting external handlers.
    if (is_zero(validation_backlog_))
        handle_event(ec, chase::bump, height_t{});
}

// neutrino
// ----------------------------------------------------------------------------
// Since neutrino filters must be computed in block order, and require full
// block and previous output deserialization, configuration of this option
// implies either ordered validation or re-deserialization of 115% of the store
// in another chaser or populated block caching in that chaser (which implies a
// controlled/narrow window of concurrent validation).

// Returns null_hash if not found, intended for genesis block.
hash_digest chaser_validate::get_neutrino(size_t height) const NOEXCEPT
{
    hash_digest neutrino{};
    const auto& query = archive();
    if (query.neutrino_enabled())
    {
        // candidate regression race may result in null_hash, which is ok, as
        // in that case position will subsequently be reset to below height.
        query.get_filter_head(neutrino, query.to_candidate(height));
    }

    return neutrino;
}

// This can only fail if block missing or prevouts are not fully populated.
bool chaser_validate::update_neutrino(const header_link& link) NOEXCEPT
{
    const auto& query = archive();
    if (!query.neutrino_enabled())
        return true;

    // Avoid computing the filter if already stored.
    if (!query.to_filter(link).is_terminal())
        return true;

    const auto block_ptr = query.get_block(link);
    if (!block_ptr)
        return false;

    const auto& block = *block_ptr;
    return query.populate(block) && update_neutrino(link, block);
}

// This can only fail if prevouts are not fully populated.
bool chaser_validate::update_neutrino(const header_link& link,
    const chain::block& block) NOEXCEPT
{
    auto& query = archive();
    if (!query.neutrino_enabled())
        return true;

    data_chunk filter{};
    if (!compute_filter(filter, block))
        return false;

    neutrino_ = compute_filter_header(neutrino_, filter);
    return query.set_filter(link, neutrino_, filter);
}

// position
// ----------------------------------------------------------------------------

void chaser_validate::update_position(size_t height) NOEXCEPT
{
    set_position(height);
    neutrino_ = get_neutrino(position());
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
