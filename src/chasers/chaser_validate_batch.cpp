/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

#include <algorithm>
#include <mutex>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::chain;
using namespace database;
using namespace std::chrono;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// TODO: ecdsa can be retained, as they don't fault, so set batched_ here.
// TODO: scnorr can be retained if each threshold carries total sig count.
// TODO: if so we can detect faulted (ignore) if full set is missing for block.
// Cannot know if archived batch is faulted, despite being otherwise full, as
// faulted is a non-persistent state. So we must purge batches at start.
code chaser_validate::start_batch() NOEXCEPT
{
    auto& query = archive();
    return (batch_enabled_ &&
        (!query.purge_ecdsa_signatures() ||
         !query.purge_schnorr_signatures())) ?
        error::batch1 : error::success;
}

void chaser_validate::push_batch(const header_link& link, size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (closed()) return;
    batched_.push_back(link);

    // chase portion of notify_block(success).
    notify({}, chase::valid, possible_wide_cast<height_t>(height));

    // Process both tables when one hits target, allowing batched_ clearance
    // and therefore forward confirmation progress.
    process_batch(false);
}

void chaser_validate::process_batch(bool residual) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Test outside of lock to prevent reader contention for nearly all calls.
    auto& query = archive();
    if (closed() || (!residual &&
        (query.ecdsa_records() < batch_target_) &&
        (query.schnorr_records() < batch_target_)))
        return;

    // Unique lock prevents batch table updates during evaluation, allowing the
    // tables to be fully purged upon completion, and ensuring that evaluation
    // does not operate over partial block records in the batch tables.
    // ========================================================================
    const std::unique_lock lock{ mutex_ };

    // Must retest inside the lock as table updates are running concurrently.
    if (closed()) return;
    const auto ecdsa = query.ecdsa_records();
    const auto schnorr = query.schnorr_records();
    if (!residual && (ecdsa < batch_target_) && (schnorr < batch_target_))
        return;

    log_captures();

    // set_block_unconfirmable(ecdsa)
    // ------------------------------------------------------------------------

    if (is_nonzero(ecdsa))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_ecdsa_signatures(stopping_, invalids))
        {
            // False return implies canceled (only).
            return;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::ecdsa_secs, elapsed);
        LOGN(log_rate("Batch verify rate ecdsa.... ", ecdsa, elapsed));

        if (!process_invalids(invalids) || !query.purge_ecdsa_signatures())
        {
            fault(error::batch2);
            return;
        }
    }

    // set_block_unconfirmable(schnorr)
    // ------------------------------------------------------------------------

    if (is_nonzero(schnorr))
    {
        header_links invalids{};
        const auto start = network::logger::now();
        if (!query.verify_schnorr_signatures(stopping_, invalids))
        {
            // False return implies canceled (only).
            return;
        }

        const auto end = network::logger::now();
        const auto elapsed = duration_cast<seconds>(end - start).count();
        fire(events::schnorr_secs, elapsed);
        LOGN(log_rate("Batch verify rate schnorr.. ", schnorr, elapsed));

        if (!process_invalids(invalids) || !query.purge_schnorr_signatures())
        {
            fault(error::batch3);
            return;
        }
    }

    // set_block_valid(batched_ excluding ecdsa/schnorr failures)
    // ------------------------------------------------------------------------

    if (!process_valids(residual))
    {
        fault(error::batch4);
        return;
    }
    // ========================================================================
}

// Invalids might not be included in batched, as link push is a race.
// Collected links are only required to set valid, not invalid, and do not
// need to coincide with the batch that is currently being processed (!).
bool chaser_validate::process_invalids(const header_links& invalids) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: invalids)
    {
        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_unconfirmable(link))
            return false;

        notify_block(system::error::invalid_signature, height, link, false);
    }

    // Set all invalids links in batched_ to terminal (to be skipped).
    if (!invalids.empty())
    {
        std::ranges::replace_if(batched_, [&](const auto& link) NOEXCEPT
        {
            return contains(invalids, link);
        }, header_link::terminal);
    }

    return true;
}

// Set all batched blocks that aren't invalid to valid.
// May be ancestors of invalid, in which case they are also unconfirmable.
bool chaser_validate::process_valids(bool residual) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto& query = archive();
    for (const auto& link: batched_)
    {
        // Terminal links are previously set invalid (to be skipped).
        if (link == header_link::terminal)
            continue;

        size_t height{};
        if (!query.get_height(height, link) ||
            !query.set_block_valid(link))
            return false;

        // logging portion of notify_block(success).
        fire(events::block_validated, height);
        LOGV("Block validated: " << height << " (batch)");
    }

    batched_.clear();
    if (residual)
        batched_.shrink_to_fit();

    return true;
}

signatures chaser_validate::get_capture(const header_link& link) NOEXCEPT
{
    if (!batch_enabled_ || is_current_header(link))
        return { false };

    const auto sequence = to_shared<atomic_counter>();
    return signatures
    {
        .enabled = true,
        .log = BIND_THIS(do_log, _1),
        .fire = BIND_THIS(do_fire, _1, _2),
        .ecdsa = BIND_THIS(do_ecdsa, _1, _2, _3, link),
        .schnorr = BIND_THIS(do_schnorr, _1, _2, _3, link),
        .multisig = BIND_THIS(do_multisig, _1, _2, _3, link, sequence),
        .threshold = BIND_THIS(do_threshold, _1, link, sequence)
    };
}

// private
// ----------------------------------------------------------------------------

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
            missed_ecdsa_ += count;
            break;
        case missed::multisig:
            missed_multisig_ += count;
            break;
        case missed::schnorr:
            missed_schnorr_ += count;
            break;
        default:;
    }
}

bool chaser_validate::do_ecdsa(const hash_digest& digest,
    const ec_compressed& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++ecdsa_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch5);
    return set;
}

bool chaser_validate::do_schnorr(const hash_digest& digest,
    const ec_xonly& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++schnorr_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch6);
    return set;
}

bool chaser_validate::do_multisig(const hash_digest& ,
    const ec_compresseds& points, const ec_signatures& BC_DEBUG_ONLY(signs),
    const header_link& , const atomic_counter_ptr& ) NOEXCEPT
{
    BC_ASSERT(points.size() == signs.size());

    multisig_ += points.size();
    ////const auto set = archive().set_signatures(digest, points, signs,
    ////    (*sequence)++, link);
    ////if (!set) fault(error::batch7);
    ////return set;
    return true;
}

bool chaser_validate::do_threshold(const threshold_group& group,
    const header_link& , const atomic_counter_ptr& ) NOEXCEPT
{
    threshold_ += group.entries.size();
    ////const auto set = archive().set_signatures(group, (*sequence)++, link);
    ////if (!set) fault(error::batch8);
    ////return set;
    return true;
}

std::string chaser_validate::log_rate(const std::string& name,
    size_t numerator, size_t denominator) const NOEXCEPT
{
    const auto rate = numerator / greater(denominator, one);
    return (boost_format("%1% (%2% / %3%) = %4% sps") %
        name % numerator % denominator % rate).str();
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
    LOGV(log_ratio("Capture rate ecdsa.... ", ecdsa_,     ecdsa_     + missed_ecdsa_));
    LOGV(log_ratio("Capture rate multisig. ", multisig_,  multisig_  + missed_multisig_));
    LOGV(log_ratio("Capture rate schnorr.. ", schnorr_,   schnorr_   + missed_schnorr_));
    LOGV(log_ratio("Capture rate threshold ", threshold_, threshold_ + zero));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
