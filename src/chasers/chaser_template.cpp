/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/node/chasers/chaser_template.hpp>

#include <atomic>
#include <utility>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_template

using namespace system;
using namespace network;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_template::chaser_template(full_node& node) NOEXCEPT
  : chaser(node)
{
}

// start/stop
// ----------------------------------------------------------------------------

code chaser_template::start() NOEXCEPT
{
    // Construct is too early to create the unstarted timer.
    timer_ = emplace_shared<deadline>(log, strand());

    SUBSCRIBE_CHASE(handle_chase, _1, _2);
    return error::success;
}

void chaser_template::stopping(const code& ec) NOEXCEPT
{
    POST(do_stopping, ec);
}

void chaser_template::do_stopping(const code&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (timer_)
    {
        timer_->stop();
        timer_.reset();
    }

    complete(network::error::service_stopped);
}

// event handlers
// ----------------------------------------------------------------------------

bool chaser_template::handle_chase(const code&, event_value value) NOEXCEPT
{
    if (closed())
        return false;

    switch (to_chase(value))
    {
        case chase::organized:
        case chase::reorganized:
        {
            POST(do_moved, false);
            break;
        }
        case chase::block_settled:
        {
            POST(do_moved, true);
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

// The settled block has been applied to the pool, so its template is built
// if demanded, and otherwise upon the next request.
void chaser_template::do_moved(bool settled) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (closed())
        return;

    ++moves_;
    if (settled && is_nonzero(demand_))
        build();
}

// methods
// ----------------------------------------------------------------------------

uint64_t chaser_template::template_generation() const NOEXCEPT
{
    return generation_.load(std::memory_order_relaxed);
}

void chaser_template::get_template(block_template_handler&& handler) NOEXCEPT
{
    if (closed())
        return;

    POST(do_get_template, std::move(handler));
}

void chaser_template::do_get_template(
    const block_template_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed())
    {
        handler(network::error::service_stopped, {});
        return;
    }

    if (held_ && (moved_ == moves_) && (built_ == pool_generation()))
    {
        handler(error::success, held_);
        return;
    }

    waiting_.push_back(handler);
    build();
}

void chaser_template::demand_template(bool demand) NOEXCEPT
{
    if (closed())
        return;

    POST(do_demand_template, demand);
}

void chaser_template::do_demand_template(bool demand) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (demand)
    {
        if (is_zero(demand_++))
            start_timer();
    }
    else if (is_nonzero(demand_) && is_zero(--demand_) && timer_)
    {
        timer_->stop();
    }
}

// build
// ----------------------------------------------------------------------------

// private
void chaser_template::build() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (building_)
        return;

    building_ = true;
    request_template(
        BIND(handle_build, _1, _2, _3, _4, pool_generation(), moves_));
}

// private
void chaser_template::handle_build(const code& ec,
    const chain::context& context, const hash_digest& previous,
    const database::tx_links& links, uint64_t generation,
    size_t moves) NOEXCEPT
{
    POST(do_build, ec, context, previous, links, generation, moves);
}

// private
// A build that the chain moved under is not of its top, so is built again.
void chaser_template::do_build(const code& ec,
    const chain::context& context, const hash_digest& previous,
    const database::tx_links& links, uint64_t generation,
    size_t moves) NOEXCEPT
{
    BC_ASSERT(stranded());
    building_ = false;

    if (closed())
    {
        complete(network::error::service_stopped);
        return;
    }

    if (ec == network::error::service_stopped ||
        ec == error::pooling_disabled)
    {
        held_.reset();
        complete(ec);
        return;
    }

    if (ec)
    {
        complete(fault(ec));
        return;
    }

    if (moves != moves_)
    {
        build();
        return;
    }

    built_ = generation;
    if (const auto failed = replace(context, previous, links))
    {
        complete(fault(failed));
        return;
    }

    moved_ = moves;
    complete(error::success);
}

// private
// The template held is replaced when it is not of the chain top, or when the
// fees of the build exceed its own by more than the configured increase.
code chaser_template::replace(const chain::context& context,
    const hash_digest& previous, const database::tx_links& links) NOEXCEPT
{
    BC_ASSERT(stranded());
    database::template_txs txs{};
    if (const auto ec = archive().get_template_txs(txs, links))
        return ec;

    uint64_t fees{};
    for (const auto& tx: txs)
        fees = ceilinged_add(fees, tx.fee);

    const auto increase = node_settings().pool.template_fee_increase;
    const auto current = held_ && (moved_ == moves_) &&
        (held_->previous == previous);

    if (current && (floored_subtract(fees, held_->fees) <= increase))
        return error::success;

    // Leaves of the coinbase are null, as it is not of the template.
    hashes txids{ null_hash };
    hashes wtxids{ null_hash };
    txids.reserve(add1(txs.size()));
    wtxids.reserve(add1(txs.size()));
    for (const auto& tx: txs)
    {
        txids.push_back(tx.txid);
        wtxids.push_back(tx.wtxid);
    }

    const auto root = merkle_root(std::move(wtxids));
    const auto sequence = add1(template_generation());
    held_ = to_shared<block_template>(block_template
    {
        previous,
        context,
        sequence,
        fees,
        links,
        sha256::double_hash(root, null_hash),
        chain::block::merkle_branch(zero, std::move(txids)),
        std::move(txs)
    });

    generation_.store(sequence, std::memory_order_relaxed);
    fire(events::template_issued, links.size());
    notify(error::success, chases::template_{ context.height });
    return error::success;
}

// private
void chaser_template::complete(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto handlers = std::exchange(waiting_, {});
    for (const auto& handler: handlers)
        handler(ec, ec ? block_template::cptr{} : held_);
}

// timer
// ----------------------------------------------------------------------------

// private
void chaser_template::start_timer() NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto interval = node_settings().pool.template_interval();
    if (!timer_ || is_zero(interval.count()))
        return;

    timer_->start(BIND(handle_timer, _1),
        duration_cast<deadline::duration>(interval));
}

// private
void chaser_template::handle_timer(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed() || !timer_ || ec == network::error::operation_canceled)
        return;

    if (ec && ec != network::error::operation_timeout)
    {
        LOGF("Template chaser timer fault, " << ec.message());
        return;
    }

    if (is_zero(demand_))
        return;

    if ((moved_ != moves_) || (built_ != pool_generation()))
        build();

    start_timer();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
