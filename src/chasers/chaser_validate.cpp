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
using namespace system::chain;
using namespace system::neutrino;
using namespace database;
using namespace std::placeholders;

// Shared pointer is required to keep the race object alive in bind closure.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_blocks_(node.config().bitcoin.subsidy_interval_blocks),
    threadpool_(std::max(node.config().node.threads, 1_u32))
{
}

code chaser_validate::start() NOEXCEPT
{
    update_position(archive().get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating message/query traffic from the candidate chain.
    if (suspended())
        return true;

    // These come out of order, advance in order asynchronously.
    // Asynchronous completion results in out of order notification.
    switch (event_)
    {
        // Track downloaded.
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            POST(do_checked, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::regressed:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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

    // TODO: update specialized fault codes.

    // Validate checked blocks starting immediately after last validated.
    for (auto height = add1(position()); !closed(); ++height)
    {
        // Precondition (associated).
        // ....................................................................

        // Validation is always sequential from position, along the candidate
        // index. It does not care about regressions that may be in process.
        const auto link = query.to_candidate(height);

        const auto ec = query.get_block_state(link);
        if (ec == database::error::integrity)
        {
            fault(error::node_validate);
            return;
        }

        if (ec == database::error::unassociated)
        {
            // Wait until the gap is filled.
            return;
        }

        if (ec == database::error::block_unconfirmable)
        {
            LOGR("Unconfirmable block [" << height << "] " << ec.message());
            fire(events::block_unconfirmable, height);
            notify(ec, chase::unvalid, link);
            return;
        }

        // These are cheap, so do even though checkpoint overlaps bypassed.
        const auto malleable64 = query.is_malleable64(link);
        auto bypass = (
            ec == database::error::block_valid ||
            ec == database::error::block_confirmable ||
            (is_under_checkpoint(height) && !malleable64));

        // malleable64 overrides bypass state because it's set from header only.
        if (!bypass && !malleable64 && !query.get_bypass(bypass, link))
        {
            fault(database::error::integrity);
            return;
        }

        if (bypass)
        {
            update_position(height);
            ////fire(events::validate_bypassed, height);
            notify(ec, chase::valid, height);
            continue;
        }

        // Skipped validation here always succeeds but doesn't set block_valid.
        // So this will always start reporting after the top block_confirmable.

        // TODO: validation.
        ////// TODO: the quantity of work must be throttled.
        ////// This will very rapidly pump all outstanding work into asio queue.
        ////if (!enqueue_block(link))
        ////{
        ////    fault(error::node_validate);
        ////    return;
        ////}

        // Retain last height in validation sequence, update neutrino.
        update_position(height);
        fire(events::block_validated, height);
        notify(ec, chase::valid, height);
    }
}

// DISTRUBUTE WORK UNITS
bool chaser_validate::enqueue_block(const header_link& link) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    database::context context{};
    const auto txs = query.to_txs(link);
    if (txs.empty() || !query.get_context(context, link))
        return false;

    // race_unity: last to finish with success, or first error code.
    const auto racer = std::make_shared<race>(txs.size());
    racer->start(BIND(handle_txs, _1, _2, link, context));

    fire(events::block_buffered, context.height);
    for (auto tx = txs.begin(); !closed() && tx != txs.end(); ++tx)
        boost::asio::post(threadpool_.service(),
            std::bind(&chaser_validate::validate_tx,
                this, context, *tx, racer));

    return true;
}

// START WORK UNIT
void chaser_validate::validate_tx(const database::context& context,
    const tx_link& link, const race::ptr& racer) NOEXCEPT
{
    if (closed())
    {
        POST(handle_tx, network::error::service_stopped, link, racer);
        return;
    }

    auto& query = archive();
    auto ec = query.get_tx_state(link, context);

    // These states bypass validation.
    if (ec == database::error::integrity ||
        ec == database::error::tx_connected ||
        ec == database::error::tx_disconnected)
    {
        POST(handle_tx, ec, link, racer);
        return;
    }

    // These other states imply validation is required.
    //// database::error::tx_preconnected
    //// database::error::unknown_state
    //// database::error::unvalidated

    const chain::context ctx
    {
        context.flags,  // [accept & connect]
        {},             // timestamp
        {},             // mtp
        context.height, // [accept]
        {},             // minimum_block_version
        {}              // work_required
    };

    const auto set_connected = [&query, &ctx, &link, &context](
        const transaction& tx) NOEXCEPT
    {
        const auto bip16 = ctx.is_enabled(flags::bip16_rule);
        const auto bip141 = ctx.is_enabled(flags::bip141_rule);
        const auto sigops = tx.signature_operations(bip16, bip141);

        // TODO: cache fee and sigops from validation stage.
        return query.set_tx_connected(link, context, tx.fee(), sigops) ?
            error::success : error::store_integrity;
    };

    const auto set_disconnected = [&query, &link, &context](
        const code& invalid) NOEXCEPT
    {
        return query.set_tx_disconnected(link, context) ? invalid :
            error::store_integrity;
    };

    code invalid{};
    const auto tx = query.get_transaction(link);
    if (!tx)
    {
        ec = error::store_integrity;
    }
    else if (!query.populate(*tx))
    {
        ec = set_disconnected(system::error::missing_previous_output);
        fire(events::tx_invalidated, ctx.height);
    }
    else if (((invalid = tx->accept(ctx))) || ((invalid = tx->connect(ctx))))
    {
        ec = set_disconnected(invalid);
        fire(events::tx_invalidated, ctx.height);

        LOGR("Invalid tx [" << encode_hash(tx->hash(false)) << "] in block ("
            << ctx .height << ") " << invalid.message());
    }
    else
    {
        ec = set_connected(*tx);
    }

    POST(handle_tx, ec, link, racer);
}

// FINISH WORK UNIT
void chaser_validate::handle_tx(const code& ec, const tx_link& tx,
    const race::ptr& racer) NOEXCEPT
{
    BC_ASSERT(stranded());

    // handle_txs will only get invoked once, with a first error code, so
    // invoke fault here ensure that non-validation codes are not lost.
    if (ec == error::store_integrity || ec == database::error::integrity)
        fault(error::node_validate);

    // TODO: need to sort out bypass, validity, and fault codes.
    // Always allow the racer to finish, invokes handle_txs exactly once.
    racer->finish(ec, tx);
}

// SYNCHRONIZE WORK UNITS
void chaser_validate::handle_txs(const code& ec, const tx_link& tx,
    const header_link& link, const database::context& ctx) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    if (ec)
    {
        // Log tx here as it's the first failed one.
        LOG_ONLY(const auto hash = encode_hash(archive().get_tx_key(tx));)
        LOGR("Error validating tx [" << hash << "] " << ec.message());
    }

    validate_block(ec, link, ctx);
}

// SUMMARIZE WORK
void chaser_validate::validate_block(const code& ec,
    const header_link& link, const database::context& ctx) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    if (ec)
    {
        ////// Transactions are set strong upon archive when under bypass. Only
        ////// malleable blocks are validated under bypass, and not set strong.
        ////if (is_bypassed(ctx.height))
        ////{
        ////    LOGR("Malleated64 block [" << ctx.height << "] " << ec.message());
        ////    notify(ec, chase::malleated, link);
        ////    fire(events::block_malleated, ctx.height);
        ////    return;
        ////}

        if (!query.set_block_unconfirmable(link))
        {
            fault(error::set_block_unconfirmable);
            return;
        }

        LOGR("Unconfirmable block [" << ctx.height << "] " << ec.message());
        notify(ec, chase::unconfirmable, link);
        fire(events::block_unconfirmable, ctx.height);
        return;
    }

    if (!query.set_block_valid(link))
    {
        fault(error::set_block_valid);
        return;
    }

    // TODO: collect fees and sigops for block validate with no block.

    // fire event first so that log is ordered.
    fire(events::block_validated, ctx.height);
    notify(ec, chase::valid, ctx.height);

    LOGV("Block.txs accepted and connected: " << ctx.height);
}

// neutrino
// ----------------------------------------------------------------------------

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
