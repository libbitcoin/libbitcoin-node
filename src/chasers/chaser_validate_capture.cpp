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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::chain;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Capture handlers.
// ----------------------------------------------------------------------------
// private

void chaser_validate::do_log(const script& ) NOEXCEPT
{
    // Enable for a game of whack-a-mole.
    ////LOGA("Sigop @ " << ctx.height << " -> "
    ////    << missed.to_string(flags::all_rules));
}

void chaser_validate::do_fire(missed miss, size_t count) NOEXCEPT
{
    switch (miss)
    {
        case missed::ecdsa:
            counters_.missed_ecdsa_ += count;
            break;
        case missed::multisig:
            counters_.missed_multisig_ += count;
            break;
        case missed::schnorr:
            counters_.missed_schnorr_ += count;
            break;
        default:;
    }
}

// Capture helpers.
// ----------------------------------------------------------------------------
// private

signatures chaser_validate::get_capture(const header_link& link) NOEXCEPT
{
    if (!batch_enabled_ || link.is_terminal() || is_current_header(link))
        return { false };

    // The capture populates this thread's accumulators (commit_capture consumes).
    return signatures
    {
        .enabled = true,
        .log = BIND_THIS(do_log, _1),
        .fire = BIND_THIS(do_fire, _1, _2)
    };
}

// Commit this thread's captured signatures as the block's batch rows. All
// batch table state is written inside the commit epoch (turnstile). When
// diverted by a drain, or upon store decline, the rows are verified in
// place, equivalent to the inline evaluation their capture fabricated
// (batched is cleared so the block completes by the non-batched path).
code chaser_validate::commit_capture(bool& batched,
    const header_link& link) NOEXCEPT
{
    code ec{};
    auto committed = false;
    auto& ecdsa = signatures::ecdsa_rows();
    auto& schnorr = signatures::schnorr_rows();

    if (enter_capture())
    {
        auto& query = archive();
        committed =
            query.set_signatures(ecdsa, link) &&
            query.set_signatures(schnorr, link) &&
            query.set_prevalid(link);
        exit_capture();

        // Store decline (e.g. disk full), recoverable once faulted.
        if (!committed)
            fault(error::batch5);
    }

    const auto thresholds = schnorr.thresholds();
    const auto singles = schnorr.rows().size() - thresholds;

    if (committed)
    {
        counters_.ecdsa_ += ecdsa.singles();
        counters_.multisig_ += ecdsa.multisig_keys();
        counters_.schnorr_ += singles;
        counters_.threshold_ += thresholds;
    }
    else
    {
        counters_.missed_ecdsa_ += ecdsa.singles();
        counters_.missed_multisig_ += ecdsa.multisig_keys();
        counters_.missed_schnorr_ += singles;
        counters_.missed_threshold_ += thresholds;

        // Block validity remains fully determined.
        batched = false;
        if (!ecdsa.verify() || !schnorr.verify())
            ec = system::error::invalid_signature;
    }

    ecdsa.clear();
    schnorr.clear();
    return ec;
}

// Discard this thread's captured signatures (script failure preempted commit).
void chaser_validate::clear_capture() NOEXCEPT
{
    signatures::ecdsa_rows().clear();
    signatures::schnorr_rows().clear();
}

// Release all threads' accumulator capacity (batching subsided). Safe on the
// strand with an empty backlog: accumulators are populated only within
// validate_block, and the strand is the sole poster of validations.
void chaser_validate::do_purge_capture() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (is_zero(validate_backlog_.load()))
        signatures::purge();
}

std::string chaser_validate::log_ratio(const std::string& name,
    size_t numerator, size_t denominator) const NOEXCEPT
{
    if (is_zero(denominator))
        return name;

    const auto ratio = (100.0 * numerator) / denominator;
    return (boost_format("%1% (%2% / %3%) = %4$.4f%%") %
        name % numerator % denominator % ratio).str();
}

void chaser_validate::log_captures() const NOEXCEPT
{
    LOGN(log_ratio("Capture ecdsa....", counters_.ecdsa_,
        counters_.ecdsa_     + counters_.missed_ecdsa_));
    LOGN(log_ratio("Capture multisig.", counters_.multisig_,
        counters_.multisig_  + counters_.missed_multisig_));
    LOGN(log_ratio("Capture schnorr..", counters_.schnorr_,
        counters_.schnorr_   + counters_.missed_schnorr_));
    LOGN(log_ratio("Capture threshold", counters_.threshold_,
        counters_.threshold_ + counters_.missed_threshold_));
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
